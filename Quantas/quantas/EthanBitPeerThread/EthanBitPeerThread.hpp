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

#ifndef ETHANBITPEERTHREAD_HPP
#define ETHANBITPEERTHREAD_HPP

#include "../Common/Peer.hpp"
#include "../Common/Simulation.hpp"
#include <atomic>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace quantas {

using std::mutex;
using std::ostream;
using std::set;
using std::shared_lock;
using std::shared_mutex;
using std::unique_lock;
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
    int blockID = -1;               // Unique identifier for the block
    int parentBlockID = -1;         // Reference to parent (previous) block
    int height = 1;                 // Height of the block in the chain
};

// Message Structure
struct bitcoinMessage {
    bitcoinBlock block; // Block being sent
    bool mined = false; // True if mined block, false if transaction
};

// Statistics for tracking chain switches and flipped blocks
struct ChainStats {
    std::atomic<int> totalSwitches{
        0
    }; // total number of times the miner switched chains
    std::atomic<int> totalFlippedBlocks{0}; // total number of blocks flipped
    vector<int> currentChain;               // current chain being followed
    vector<vector<int>> switchHistory;      // history of chain switches
    std::unordered_map<int, int>
        flipFrequency; // Tracks frequency of # of blocks flipped at a given
                       // switch
    mutable mutex chainMutex; // protect chain data
};

class EthanBitPeer : public Peer<bitcoinMessage> {
  public:
    // Constructors and Destructor
    EthanBitPeer(long id);
    EthanBitPeer(const EthanBitPeer &rhs);
    ~EthanBitPeer();

    // Core Functions
    void performComputation();
    void endOfRound(const vector<Peer<bitcoinMessage> *> &_peers);

    // Output Functions
    void log() const { printTo(*_log); };
    ostream &printTo(ostream &) const;
    friend ostream &operator<<(ostream &, const EthanBitPeer &);

  private:
    // Blockchain Data Structures
    vector<int> longestChain;                // Current longest chain
    unordered_map<int, bitcoinBlock> blocks; // All blocks by ID
    unordered_map<int, vector<int>>
        edges;     // Parent blocks with children (forks)
    set<int> tips; // Blocks with no children
    std::atomic<int> longestChainLength{1};

    // Mutexes to protect data structures
    mutable shared_mutex chainMutex;
    mutable mutex blocksMutex;
    mutable mutex edgesMutex;
    mutable mutex tipsMutex;

    // Chain Tracking Structures
    unordered_map<long, ChainStats> minerStats; // Stats for each miner
    mutable mutex minerStatsMutex;

    // Transaction & Block Management
    vector<bitcoinBlock> unlinkedBlocks; // Blocks waiting to be linked
    vector<bitcoinBlock> transactions;   // Received transactions
    mutable mutex unlinkedBlocksMutex;
    mutable mutex transactionsMutex;

    static std::atomic<int> blockCounter;       // Global block counter
    static std::atomic<int> currentTransaction; // Current transaction ID
    static mutex currentTransaction_mutex;      // Mutex for transaction ID

    // Mining Parameters
    int submitRate = 10; // Transaction submission rate
    int mineRate = 10;   // Block mining rate

    void printFrequencyData() const;
    void printBlockChain() const;
    // Core Operations
    void checkIncomingMessages();       // Process incoming messages
    void linkBlocks();                  // Link blocks to blockchain
    bool checkSubmitTrans();            // Check if should submit transaction
    void submitTrans();                 // Submit new transaction
    bool checkMineBlock();              // Check if should mine
    void mineBlocks();                  // Mine new blocks
    bitcoinBlock findNextTransaction(); // Find next transaction to mine
    void updateLongestChain();          // Update longest chain

    // Chain Tracking Operations
    void
    trackChainSwitch(const vector<int> &oldChain, const vector<int> &newChain);
    int countFlippedBlocks(
        const vector<int> &oldChain, const vector<int> &newChain
    );
    int getLongestChainTip() const; // Get tip of longest chain

    // Statistics Getters
    int getTotalSwitches(long minerId) const;
    int getTotalFlippedBlocks(long minerId) const;
    const vector<int> &getCurrentChain(long minerId) const;
};

std::atomic<int> EthanBitPeer::blockCounter{0};
std::atomic<int> EthanBitPeer::currentTransaction{0};
mutex EthanBitPeer::currentTransaction_mutex;

// Simulation Generator
Simulation<bitcoinMessage, EthanBitPeer> *generateSim();

} // namespace quantas

#endif // ETHANBITPEER_HPP