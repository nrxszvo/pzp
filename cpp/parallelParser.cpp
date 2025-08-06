#include "parallelParser.h"
#include "decompress.h"
#include "parseMoves.h"
#include "parser.h"
#include "utils.h"
#include <stdexcept>
#include <chrono>
#include <iostream>
#include <unordered_set>

struct GameState {
	std::queue<std::shared_ptr<GameDataBlock> >* gamesQ;
	std::mutex* gamesMtx;
	std::condition_variable* gamesCv;
	int pid;
	GameState(
			std::queue<std::shared_ptr<GameDataBlock> >* gamesQ, 
			std::mutex* gamesMtx, 
			std::condition_variable* gamesCv) 
		: pid(-1), gamesQ(gamesQ), gamesMtx(gamesMtx), gamesCv(gamesCv) {};

};

void loadGamesZst(GameState gs, std::string zst, size_t frameStart, size_t frameEnd, int nMoveProcessors, int minSec, int maxSec, int maxInc) {
	int gameId = 0;
	int gamestart = 0;
	int lineno = 0;
	PgnProcessor processor(minSec, maxSec, maxInc);
	DecompressStream decompressor(zst, frameStart, frameEnd);
	auto games = std::make_shared<GameDataBlock>();

	while(decompressor.decompressFrame() != 0) {
		std::vector<std::string> lines;
		decompressor.getLines(lines);

		for (auto line: lines) {
			lineno++;
			auto code = processor.processLine(line);
			if (code == "COMPLETE") {
				auto gd = std::make_shared<GameData>(
					gs.pid,
					decompressor.getProgress(),
					gameId, 
					processor.getWelo(), 
					processor.getBelo(), 
					processor.getTime(),
					processor.getInc(),
					processor.getWhite(),
					processor.getBlack(),
					processor.getMoveStr(),
					zst + ":" + std::to_string(gamestart)
				);
				games->push_back(gd);
				if (games->size() == 100) {
					{
						std::lock_guard<std::mutex> lock(*gs.gamesMtx);
						gs.gamesQ->push(games);
					}
					gs.gamesCv->notify_one();
					games = std::make_shared<GameDataBlock>();
				}
				gamestart = lineno + 1;
				gameId++;
			}
		}
	}	

	if (games->size() > 0) {
		{
			std::lock_guard<std::mutex> lock(*gs.gamesMtx);
			gs.gamesQ->push(games);
		}
		gs.gamesCv->notify_one();
		games = std::make_shared<GameDataBlock>();
	}

	{
		std::lock_guard<std::mutex> lock(*gs.gamesMtx);
		games->push_back(std::make_shared<GameData>(gs.pid, "FILE_DONE", gameId));
		for (int i=0; i<nMoveProcessors; i++) {
			gs.gamesQ->push(games);
		}
	}
	gs.gamesCv->notify_all();
}


std::vector<std::shared_ptr<std::thread> > startGamesReader(
		std::queue<std::shared_ptr<GameDataBlock> >& gamesQ, 
		std::mutex& gamesMtx,
	   	std::condition_variable& gamesCv,
		std::string zst,
		std::vector<size_t>& frameBoundaries,
	   	int nMoveProcessors, int minSec, int maxSec, int maxInc) {
	GameState gs(&gamesQ, &gamesMtx, &gamesCv);
	std::vector<std::shared_ptr<std::thread> > procs;
	for (int i=0; i<frameBoundaries.size()-1; i++) {
		size_t start = frameBoundaries[i];
		size_t end = frameBoundaries[i+1];
		gs.pid = i;
		procs.push_back(
				std::make_shared<std::thread>(
					loadGamesZst, gs, zst, start, end, nMoveProcessors, minSec, maxSec, maxInc
					)
				);
	}
	return procs;
}

struct ProcessorState {
	std::queue<std::shared_ptr<GameDataBlock> >* gamesQ;
	std::queue<std::shared_ptr<MoveDataBlock> >* outputQ;
	int pid;
	std::mutex* gamesMtx;
	std::mutex* outputMtx;
	std::condition_variable* gamesCv;
	std::condition_variable* outputCv;
	ProcessorState(
			std::queue<std::shared_ptr<GameDataBlock> >* gamesQ, 
			std::queue<std::shared_ptr<MoveDataBlock> >* outputQ, 
			std::mutex* gamesMtx, 
			std::mutex* outputMtx,
		   	std::condition_variable* gamesCv,
		   	std::condition_variable* outputCv) 
		: gamesQ(gamesQ), 
		outputQ(outputQ),
	   	pid(-1),
	   	gamesMtx(gamesMtx),
	   	outputMtx(outputMtx),
	   	gamesCv(gamesCv),
	   	outputCv(outputCv) {};
};

void processGames(ProcessorState ps, int nReaders) {		
	int nReadersDone = 0;
	std::unordered_set<int> readerPids;
	int totalGames = 0;
	while(true) {
		std::shared_ptr<GameDataBlock> games;
		{
			std::unique_lock<std::mutex> lock(*ps.gamesMtx);
			ps.gamesCv->wait(lock, [&]{return !ps.gamesQ->empty();});
			games = ps.gamesQ->front();
			if (games->back()->info == "FILE_DONE") {
				if (!readerPids.contains(games->back()->pid)) {
					readerPids.insert(games->back()->pid);
					nReadersDone++;
					totalGames += games->back()->gameId;
					ps.gamesQ->pop();
				}
			} else {
				ps.gamesQ->pop();
			}
			lock.unlock();
		}
		auto moves = std::make_shared<MoveDataBlock>();
		if (games->back()->info == "FILE_DONE") {
			if (nReadersDone==nReaders) {
				std::lock_guard<std::mutex> lock(*ps.outputMtx);
				moves->push_back(std::make_shared<MoveData>(ps.pid, "DONE", totalGames));
				ps.outputQ->push(moves);
				ps.outputCv->notify_one();
				break;
			}
		} else {
			for (auto gd: *games) {
				try {
					auto [mvs, clk, eval, result] = parseMoves(gd->moveStr);
					moves->push_back(
						std::make_shared<MoveData>(gd->pid, gd, mvs, clk, eval, result)
					);
				} catch(std::exception &e) {
					moves->push_back(std::make_shared<MoveData>(gd->pid, "INVALID"));
				}
			}
			{
				std::lock_guard<std::mutex> lock(*ps.outputMtx);
				ps.outputQ->push(moves);
				ps.outputCv->notify_one();
			}
		}	
	}
}

std::vector<std::shared_ptr<std::thread> >  startProcessorThreads(
		int nMoveProcessors,
		int nReaders, 
		std::queue<std::shared_ptr<GameDataBlock> >& gamesQ, 
		std::queue<std::shared_ptr<MoveDataBlock> >& outputQ, 
		std::mutex& gamesMtx, 
		std::mutex& outputMtx,
	   	std::condition_variable& gamesCv, 
		std::condition_variable& outputCv) {

	std::vector<std::shared_ptr<std::thread> > threads;
	ProcessorState ps(&gamesQ, &outputQ, &gamesMtx, &outputMtx, &gamesCv, &outputCv);
	for (int i=0; i<nMoveProcessors; i++) {
		ps.pid = i;
		threads.push_back(std::make_shared<std::thread>(processGames, ps, nReaders));
	}	
	return threads;
}

ParallelParser::ParallelParser(int nReaders, int nMoveProcessors, int minSec, int maxSec, int maxInc, std::shared_ptr<EloWriter> writer, size_t chunkSize) 
	: nReaders(nReaders), nMoveProcessors(nMoveProcessors), minSec(minSec), maxSec(maxSec), maxInc(maxInc), writer(writer), chunkSize(chunkSize) {};


int64_t ParallelParser::parse(std::string zst, std::string name, int offset, int printFreq, std::mutex& print_mtx, std::vector<std::string>& info) {
	int64_t ngames = 0;
	int64_t nValidGames = 0;
	std::vector<int64_t> nGamesLastUpdate(nReaders, 0);
	int totalGames = INT_MAX;
	int nFinished = 0;
	
	std::vector<size_t> frameBoundaries = getFrameBoundaries(zst, nReaders);
	nReaders = frameBoundaries.size()-1;

	procThreads = startProcessorThreads(
		nMoveProcessors,
		nReaders, 
		gamesQ, 
		outputQ, 
		gamesMtx,
		outputMtx,
		gamesCv,
		outputCv
	);

	gameThreads = startGamesReader(
		gamesQ, 
		gamesMtx, 
		gamesCv, 
		zst,
		frameBoundaries,
		nMoveProcessors,
		minSec,
		maxSec,
		maxInc
	);

	auto start = hrc::now();
	auto lastPrintTime = std::vector<hrc::time_point>(nReaders, start);
		
	std::shared_ptr<ParsedData> output = std::make_shared<ParsedData>(chunkSize);
	int curOutputNgames = 0;

	while (ngames < totalGames || nFinished < nMoveProcessors) {
		std::shared_ptr<MoveDataBlock> moves;
		{
			std::unique_lock<std::mutex> lock(outputMtx);
			outputCv.wait(lock, [&]{return !outputQ.empty();});
			moves = outputQ.front();
			outputQ.pop();
			lock.unlock();
		}
		for (auto md: *moves) {
			if (md->info == "DONE") {
				totalGames = md->gameId;
				nFinished++;
			} else if (md->info == "ERROR") {
				ngames++;
			} else if (md->info == "INVALID") {
				ngames++;
			} else if (md->info == "GAME") {
				output->welos[curOutputNgames] = md->welo;
				output->belos[curOutputNgames] = md->belo;
				output->whites[curOutputNgames] = md->white;
				output->blacks[curOutputNgames] = md->black;
				output->timeCtl[curOutputNgames] = md->time;
				output->increment[curOutputNgames] = md->inc;
				output->mvs[curOutputNgames] = md->mvs;
				output->clk[curOutputNgames] = md->clk;
				output->eval[curOutputNgames] = md->eval;
				output->result[curOutputNgames] = md->result;
				ngames++;
				nValidGames++;
				curOutputNgames++;
		
				if (curOutputNgames == chunkSize) {
					writer->queueBatch(output);
					output = std::make_shared<ParsedData>(chunkSize);
					curOutputNgames = 0;
				}

				if (ellapsedGTE(lastPrintTime[md->pid], printFreq)) {
					int totalGamesEst = ngames / md->progress;
					auto [eta, now] = getEta(totalGamesEst, ngames, start);
					long ellapsed = std::chrono::duration_cast<milli>(now-lastPrintTime[md->pid]).count();
					int gamesPerSec = 1000*(ngames-nGamesLastUpdate[md->pid])/ellapsed;
					nGamesLastUpdate[md->pid] = ngames;
					lastPrintTime[md->pid] = now;
					
					std::string status = "\t" + name + "." + std::to_string(md->pid) + ": " + std::to_string(int(100*md->progress)) + \
										"% done, games/sec: " + std::to_string(gamesPerSec) + ", eta: " + eta;
					int thisOffset = offset + md->pid + 1;
					{
                        //std::lock_guard<std::mutex> lock(print_mtx);
						info[thisOffset] = status;
					}
				}
			} else {
				throw std::runtime_error("invalid code: " + md->info);
			}
		}
	}
	if (curOutputNgames > 0) {
		writer->queueBatch(output);
	}
	for (auto gt: gameThreads) {
		gt->join();
	}
	for (auto rt: procThreads) {
		rt->join();
	}
	return nValidGames;
}
