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

#ifndef EthanPeer_hpp
#define EthanPeer_hpp

#include "../Common/Peer.hpp"
#include "../Common/Simulation.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace quantas {

using std::mutex;
using std::ostream;
using std::string;
using std::unordered_map;
using std::vector;

struct bitcoinTransaction {
    int id = -1;             // the transaction id
    int senderID = -1;       // ID of the sender
    int recieverID = -1;     // ID of the reciever
    int amount = 0;          // transaction amount
    int submittedRound = -1; // round in which the transaction had been
    bool confirmed = false;  // submitted
};

struct bitcoinBlock {
    int minerID = -1;                            // ID of the miner
    vector<bitcoinTransaction> blockTransaction; // transaction on the block
    int tipMiner = -1; // ID of miner who mined previous block
    int parentBlockID = -1;
    int length = 1;   // length of blockchain
    int blockID = -1; // Unique Block ID
};

struct bitcoinMessage {
    bitcoinBlock block; // whats being sent
    bool mined = false; // decides if its a block or a message
};

class EthanBitPeer : public Peer<bitcoinMessage> {
  private:
    bool isByzantine;                    // determines if the peer is malicious
    vector<bitcoinBlock> blockChain;     // Main blockchain
    vector<bitcoinBlock> unlinkedBlocks; // Blocks without known parents
    unordered_map<int, bool> spentTransactions; // Spent transaction tracker
  public:
    EthanBitPeer(long);
    EthanBitPeer(const EthanBitPeer &rhs);
    ~EthanBitPeer();

    /*** 🚀 General Blockchain Functions ***/

    void checkIncomingMessages(); // Processes incoming blocks/transactions
    void linkBlocks(); // Attempts to add unlinked blocks to the blockchain
    bool shouldSubmitTransaction(
    );                        // Checks if this node should submit a transaction
    void submitTransaction(); // Creates a new transaction and broadcasts it
    bool canMineBlock();      // Determines if this node can mine a block
    void mineBlock(
    ); // Mines the next transaction, adds to blockchain, and broadcasts it
    bitcoinTransaction findNextUnminedTransaction(
    ); // Finds next unmined transaction from the longest chain

    /* Malicious Node Behavior */

    bool isMalicious;          // Flag to indicate if this node is an attacker
    void attemptDoubleSpend(); // Triggers a double-spend attack if possible
    bool isVictimOnline(int victimID); // Checks if the victim node is online
    void sendConflictingTransactions(int victimID
    ); // Sends two conflicting transactions
    void prioritizeOwnTransaction(
    ); // Ensures attacker's transaction propagates faster
    bool didAttackSucceed(
    ); // Checks if the attacker's double-spend transaction got confirmed

    /*** 🔥 Fork Tracking Functions ***/

    void detectForks();    // Detects if multiple valid chains exist
    void storeForkState(); // Keeps track of orphaned blocks for later analysis
    void resolveForkManually(
    ); // Allows for manual resolution of forks for research purposes

    /*** 🛠 Malicious Node Chance Setup ***/

    void setMaliciousChance(int chance
    ); // Sets the chance for the node to be malicious
    void initializeMaliciousNode(
    ); // Initializes a node as malicious based on the chance
};

} // namespace quantas
#endif /* ExamplePeer_hpp */
