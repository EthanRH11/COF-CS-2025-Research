// /*
// Copyright 2022

// This file is part of QUANTAS.
// QUANTAS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. QUANTAS is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details. You should have received a copy of the GNU General Public License
// along with QUANTAS. If not, see <https://www.gnu.org/licenses/>.
// */
// Copyright 2022

// This file is part of QUANTAS.
// QUANTAS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. QUANTAS is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details. You should have received a copy of the GNU General Public License
// along with QUANTAS. If not, see <https://www.gnu.org/licenses/>.
// */

// #ifndef ETHANBITPEER_HPP
// #define ETHANBITPEER_HPP

// #include "../Common/Peer.hpp"
// #include "../Common/Simulation.hpp"
// #include <mutex>
// #include <unordered_map>
// #include <unordered_set>
// #include <vector>

// namespace quantas {

// using std::mutex;
// using std::ostream;
// using std::unordered_map;
// using std::unordered_set;
// using std::vector;

// // Transaction Structure
// struct bitcoinTransaction {
//     long int id = -1;         // Transaction ID
//     int roundSubmitted = -1;  // Round when the transaction was submitted
//     bool isMalicious = false; // Marks if transaction is malicious
// };

// // Block Structure
// struct bitcoinBlock {
//     long int minerID = -1;          // ID of peer who mined this block
//     bitcoinTransaction transaction; // Transaction included in the block
//     int parentBlockID = -1;         // Reference to parent (previous) block
//     int height = 1;                 // Height of the block in the chain
//     bool isMalicious = false;       // Is block part of an attack
// };

// // Message Structure
// struct bitcoinMessage {
//     bitcoinBlock block; // Block being sent
//     bool mined = false; // True if mined block, false if transaction
// };

// class EthanBitPeer : public Peer<bitcoinMessage> {
//   public:
//     EthanBitPeer(long id);
//     EthanBitPeer(const EthanBitPeer &rhs);
//     ~EthanBitPeer();

//     void performComputation(); // Process transactions and mine blocks
//     void endOfRound(const vector<Peer<bitcoinMessage> *> &_peers
//     ); // Runs at end of each round

//     void log() const { printTo(*_log); };
//     ostream &printTo(ostream &) const;
//     friend ostream &operator<<(ostream &, const EthanBitPeer &);

//     // Blockchain Data Structure
//     unordered_map<int, bitcoinBlock> blockchain; // Key: block ID, Value:
//     Block vector<int> longestChain; // Stores the IDs of blocks in the
//     longest chain

//     // Unlinked blocks and transactions
//     vector<bitcoinBlock> unlinkedBlocks;
//     vector<bitcoinBlock> transactions;

//     // Sim Params
//     int submitRate = 10;      // Probability of submitting a transaction
//     int mineRate = 10;        // Probability of mining a block
//     bool isMalicious = false; // True if this peer is attacking
//     int messagesSent = 0;     // Track messages sent
//     int forkCount = 0;        // Track forks in the current round
//     int totalForkCount = 0;   // Track total forks

//     // Transaction ID management
//     static int currentTransactionID;
//     static mutex transactionMutex;

//     // Blockchain Operations
//     void checkIncomingMessages(); // Process incoming messages
//     bool checkSubmitTrans();      // Check if transaction should be submitted
//     void submitTrans();           // Broadcast a new transaction
//     bool checkMineBlock();        // Check if mining is possible
//     void mineBlock();             // Create and broadcast a new block
//     bitcoinBlock findNextTrans(); // Find an unmined transaction
//     void resolveForks(); // Resolve forks by selecting the longest chain

//     // Helper Functions
//     int generateBlockID(); // Generate a unique block ID
//     vector<int> getChain(int blockID
//     ) const; // Get the full chain for a given block
// };

// Simulation<bitcoinMessage, EthanBitPeer> *generateSim();

// } // namespace quantas

// #endif // ETHANBITPEER_HPP

#ifndef ETHANBITPEER_HPP
#define ETHANBITPEER_HPP

#include "../Common/Peer.hpp"
#include "../Common/Simulation.hpp"
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace quantas {

using std::mutex;
using std::ostream;
using std::set;
using std::unordered_map;
using std::unordered_set;
using std::vector;

// Transaction Structure
struct bitcoinTransaction {
    long int id = -1;        // Transaction ID
    int roundSubmitted = -1; // Round when the transaction was submitted
};

// Block Structure
struct bitcoinBlock {
    long int minerID = -1;          // ID of peer who mined this block
    bitcoinTransaction transaction; // Transaction included in the block
    int blockID = -1;
    int parentBlockID = -1; // Reference to parent (previous) block
    int height = 1;         // Height of the block in the chain
};

// Message Structure
struct bitcoinMessage {
    bitcoinBlock block; // Block being sent
    bool mined = false; // True if mined block, false if transaction
};

class EthanBitPeer : public Peer<bitcoinMessage> {
  public:
    EthanBitPeer(long);
    EthanBitPeer(const EthanBitPeer &rhs);
    ~EthanBitPeer();

    void performComputation();
    void endOfRound(const vector<Peer<bitcoinMessage> *> &_peers);

    void log() const { printTo(*_log); };
    ostream &printTo(ostream &) const;
    friend ostream &operator<<(ostream &, const EthanBitPeer &);

    // Blockchain Structure
    vector<int> longestChain;
    unordered_map<int, bitcoinBlock> blocks;
    unordered_map<int, vector<int>>
        edges;     // Parent blocks with children (forks)
    set<int> tips; // blocks with no children
    int longestChainLength = 1;

    // Transaction & Mining Functions
    vector<bitcoinBlock> unlinkedBlocks; // Blocks waiting to be linked
    vector<bitcoinBlock> transactions;   // Recieved Transaction
    int submitRate = 10;
    int mineRate = 10;
    static int currentTransaction;
    static mutex currentTransaction_mutex;

    // Specific Operation
    void checkIncomingMessages();
    void linkBlocks();
    bool checkSubmitTrans();
    void submitTrans();
    bool checkMineBlock();
    void mineBlocks();
    int getLongestChainTip() const;
    bitcoinBlock findNextTransaction();
    void updateLongestChain(); // determines longest chain tip
};

Simulation<quantas::bitcoinMessage, quantas::EthanBitPeer> *generateSim();
} // namespace quantas

#endif