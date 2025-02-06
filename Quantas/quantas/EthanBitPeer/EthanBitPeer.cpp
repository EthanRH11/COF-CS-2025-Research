/*
Copyright 2022

This file is part of QUANTAS.
QUANTAS is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. QUANTAS is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
QUANTAS. If not, see <https://www.gnu.org/licenses/>.
*/

#include "EthanBitPeer.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <unordered_map>
#include <vector>

namespace quantas {

using std::cout;
using std::endl;
using std::lock_guard;
using std::mt19937;
using std::mutex;
using std::random_device;
using std::uniform_int_distribution;

int EthanBitPeer::currentTransactionID = 0;
mutex EthanBitPeer::transactionMutex;

EthanBitPeer::EthanBitPeer(long id) : Peer(id) {}

EthanBitPeer::EthanBitPeer(const EthanBitPeer &rhs) : Peer(rhs) {}

EthanBitPeer::~EthanBitPeer() {}

void EthanBitPeer::endOfRound(const vector<Peer<bitcoinMessage> *> &_peers) {
    std::vector<EthanBitPeer *> peers;
    for (auto peer : _peers) {
        peers.push_back(static_cast<EthanBitPeer *>(peer));
    }

    int totalMinedBlocks = 0;
    int maxLength = 0;
    int roundForkCount = 0;

    std::unordered_map<int, std::unordered_set<int>> heightToBlocks;

    // Iterate through peers to track blockchain stats
    for (auto *peer : peers) {
        if (!peer->blockchain.empty()) {
            int longestChainSize = peer->longestChain.size();
            totalMinedBlocks += longestChainSize;

            if (longestChainSize > maxLength) {
                maxLength = longestChainSize;
            }

            // Fork detection: Track unique blocks per height
            for (const auto &entry : peer->blockchain) {
                int blockHeight = entry.second.height;
                int blockID = entry.first;
                heightToBlocks[blockHeight].insert(blockID);
            }
        }
    }

    // Counting forks: A fork occurs when multiple blocks exist at the same
    // height
    for (const auto &entry : heightToBlocks) {
        if (entry.second.size() > 1) {
            roundForkCount += entry.second.size() - 1;
        }
    }

    // Logging fork count and mined blocks
    LogWriter::getTestLog()["Forks"].push_back(totalMinedBlocks - maxLength);
    LogWriter::getTestLog()["blockChain_length"].push_back(maxLength);
    // LogWriter::getTestLog()["forks"].push_back(roundForkCount);

    // // Debugging output
    // std::cout << "Total Mined Blocks: " << totalMinedBlocks << "\n";
    // std::cout << "Max Chain Length: " << maxLength << "\n";
    // std::cout << "Fork Count: " << roundForkCount << "\n";
    // std::cout << "Mined Blocks - Longest Chain: "
    //           << totalMinedBlocks - maxLength << "\n";
}

void EthanBitPeer::performComputation() {
    checkIncomingMessages();

    if (checkSubmitTrans()) {
        submitTrans();
    }
    if (checkMineBlock()) {
        mineBlock();
    }
}

void EthanBitPeer::checkIncomingMessages() {
    while (!inStreamEmpty()) {
        Packet<bitcoinMessage> newMsg = popInStream();
        bitcoinMessage msg = newMsg.getMessage();

        if (msg.mined) {
            bitcoinBlock newBlock = msg.block;
            int newBlockID = generateBlockID();

            // Check if the new block creates a fork
            bool forkDetected = false;
            for (const auto &entry : blockchain) {
                if (entry.second.parentBlockID == newBlock.parentBlockID) {
                    forkDetected = true;
                    break;
                }
            }

            // Add the new block to the blockchain
            blockchain[newBlockID] = newBlock;

            // Resolve forks if necessary
            if (forkDetected) {
                resolveForks();
            }
        } else {
            transactions.push_back(msg.block);
        }
    }
}

bool EthanBitPeer::checkSubmitTrans() { return randMod(100) < submitRate; }

void EthanBitPeer::submitTrans() {
    lock_guard<mutex> lock(transactionMutex);

    bitcoinTransaction tx{currentTransactionID++, getRound(), isMalicious};

    int parentBlockID = -1; // Default for genesis block
    int chainLength = 1;    // Default for genesis block

    if (!longestChain.empty()) {
        parentBlockID = longestChain.back();
        chainLength = blockchain[parentBlockID].height + 1;
    }

    bitcoinBlock block{id(), tx, parentBlockID, chainLength, isMalicious};

    bitcoinMessage msg{block, false};
    messagesSent += neighbors().size();
    broadcast(msg);
}

bool EthanBitPeer::checkMineBlock() { return randMod(100) < mineRate; }

void EthanBitPeer::mineBlock() {
    if (transactions.empty()) {
        std::cout << "No transactions to mine" << std::endl;
        return; // No transactions to mine
    }

    // Create a new block
    bitcoinBlock newBlock;
    newBlock.minerID = id();
    newBlock.transaction = transactions.back().transaction;
    newBlock.parentBlockID = longestChain.empty() ? -1 : longestChain.back();
    newBlock.height = longestChain.empty()
                          ? 1
                          : blockchain[newBlock.parentBlockID].height + 1;

    std::cout << "Mined new block: ID = " << generateBlockID()
              << ", ParentID = " << newBlock.parentBlockID
              << ", Height = " << newBlock.height << std::endl;

    // Add the new block to the blockchain
    int newBlockID = generateBlockID();
    blockchain[newBlockID] = newBlock;

    // Update the longest chain
    longestChain.push_back(newBlockID);

    // Broadcast the new block
    bitcoinMessage msg{newBlock, true};
    messagesSent += neighbors().size();
    broadcast(msg);
}

// void EthanBitPeer::mineBlock() {
//     if (transactions.empty()) {
//         return; // No transactions to mine
//     }

//     // Get the next transaction
//     bitcoinTransaction tx = transactions.back().transaction;
//     transactions.pop_back();

//     // Create a new block
//     bitcoinBlock newBlock;
//     newBlock.minerID = id();
//     newBlock.transaction = tx;
//     newBlock.isMalicious = isMalicious;

//     // Set parent block and height
//     if (!longestChain.empty()) {
//         newBlock.parentBlockID = longestChain.back();
//         newBlock.height = blockchain[newBlock.parentBlockID].height + 1;
//     } else {
//         newBlock.parentBlockID = -1; // Genesis block
//         newBlock.height = 1;
//     }

//     // Add the new block to the blockchain
//     int newBlockID = generateBlockID();
//     blockchain[newBlockID] = newBlock;

//     // Update the longest chain
//     longestChain.push_back(newBlockID);

//     // Broadcast the new block
//     bitcoinMessage msg{newBlock, true};
//     messagesSent += neighbors().size();
//     broadcast(msg);
// }

// void EthanBitPeer::resolveForks() {
//     // Find the longest chain
//     std::vector<int> newLongestChain;
//     for (const auto &entry : blockchain) {
//         std::vector<int> chain = getChain(entry.first);
//         if (chain.size() > newLongestChain.size()) {
//             newLongestChain = chain;
//         }
//     }

//     // Update the longest chain
//     longestChain = newLongestChain;
// }

void EthanBitPeer::resolveForks() {
    std::vector<int> newLongestChain;
    for (const auto &entry : blockchain) {
        std::vector<int> chain = getChain(entry.first);
        if (chain.size() > newLongestChain.size()) {
            newLongestChain = chain;
        }
    }

    // Log the selected longest chain
    std::cout << "Selected longest chain: ";
    for (int blockID : newLongestChain) {
        std::cout << blockID << " ";
    }
    std::cout << std::endl;

    longestChain = newLongestChain;
}

int EthanBitPeer::generateBlockID() {
    static int nextBlockID = 0;
    return nextBlockID++;
}

std::vector<int> EthanBitPeer::getChain(int blockID) const {
    std::vector<int> chain;
    while (blockID != -1) {
        chain.push_back(blockID);
        blockID = blockchain.at(blockID).parentBlockID;
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

ostream &EthanBitPeer::printTo(ostream &os) const {
    os << "Peer ID: " << id() << " | Blocks: " << longestChain.size();
    return os;
}

ostream &operator<<(ostream &os, const EthanBitPeer &peer) {
    return peer.printTo(os);
}

Simulation<bitcoinMessage, EthanBitPeer> *generateSim() {
    return new Simulation<bitcoinMessage, EthanBitPeer>();
}
} // namespace quantas