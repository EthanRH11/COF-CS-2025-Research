#include "EthanBitPeerThread.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <unordered_map>
#include <vector>

namespace quantas {

using std::cout;
using std::endl;
using std::lock_guard;
using std::mutex;
using std::random_device;
using std::shared_lock;
using std::uniform_int_distribution;
using std::unique_lock;

std::atomic<int> EthanBitPeer::blockCounter{0};
std::atomic<int> EthanBitPeer::currentTransaction{1};
mutex EthanBitPeer::currentTransaction_mutex;

EthanBitPeer::~EthanBitPeer() {}
EthanBitPeer::EthanBitPeer(const EthanBitPeer &rhs) : Peer(rhs) {}
EthanBitPeer::EthanBitPeer(long id) : Peer(id) {}

void EthanBitPeer::performComputation() {
    checkIncomingMessages();
    if (checkSubmitTrans()) {
        submitTrans();
    }
    if (checkMineBlock()) {
        mineBlocks();
    }
    updateLongestChain();
}

void EthanBitPeer::linkBlocks() {
    bool linked;
    do {
        linked = false;

        // lock all required mutexes
        std::lock_guard<mutex> unlinkedLock(unlinkedBlocksMutex);

        if (unlinkedBlocks.empty()) {
            break;
        }

        std::lock_guard<mutex> blockLock(blocksMutex);
        std::lock_guard<mutex> edgesLock(edgesMutex);
        std::lock_guard<mutex> tipsLock(tipsMutex);

        for (size_t i = 0; i < unlinkedBlocks.size(); i++) {
            bitcoinBlock block = unlinkedBlocks[i];
            int parentID = block.parentBlockID;

            if (parentID == -1) {
                blocks[block.blockID] = block;
                edges[parentID].push_back(block.blockID);
                tips.insert(block.blockID);
                unlinkedBlocks.erase(unlinkedBlocks.begin() + i);
                linked = true;
                break;
            } else if (blocks.find(parentID) != blocks.end()) {
                blocks[block.blockID] = block;
                edges[parentID].push_back(block.blockID);

                tips.erase(parentID);
                tips.insert(block.blockID);

                unlinkedBlocks.erase(unlinkedBlocks.begin() + i);
                linked = true;
                break;
            }
        }
    } while (linked);
}

void EthanBitPeer::endOfRound(const vector<Peer<bitcoinMessage> *> &_peers) {
    updateLongestChain();

    // Get statistics for this miner
    int minerSwitches = getTotalSwitches(id());
    int minerFlippedBlocks = getTotalFlippedBlocks(id());

    // Calculate network-wide statistics
    int totalNetworkSwitches = 0;
    int totalNetworkFlippedBlocks = 0;

    // Lock to safely access minerStats
    std::lock_guard<mutex> statsLock(minerStatsMutex);

    // Cast peers to EthanBitPeer to access their statistics
    for (const auto &pair : minerStats) {
        totalNetworkSwitches += pair.second.totalSwitches.load();
        totalNetworkFlippedBlocks += pair.second.totalFlippedBlocks.load();
    }

    std::unordered_map<int, int> globalFlipFrequency;
    for (const auto &pair : minerStats) {
        // Lock for accessing each miner's chain data
        std::lock_guard<mutex> chainLock(pair.second.chainMutex);
        const auto &localFrequency = pair.second.flipFrequency;
        for (const auto &entry : localFrequency) {
            int blocksFlipped = entry.first;
            int frequency = entry.second;
            globalFlipFrequency[blocksFlipped] += frequency;
        }
    }

    std::vector<std::pair<int, int>> sortedFrequency(
        globalFlipFrequency.begin(), globalFlipFrequency.end()
    );
    std::sort(
        sortedFrequency.begin(), sortedFrequency.end(),
        [](const auto &a, const auto &b) { return a.first < b.first; }
    );

    std::string frequencySummary;
    for (const auto &entry : sortedFrequency) {
        frequencySummary +=
            std::to_string(entry.first) +
            (entry.first == 1 ? " block flipped: " : " blocks flipped: ") +
            std::to_string(entry.second) + ", ";
    }
    if (!frequencySummary.empty()) {
        frequencySummary.pop_back();
        frequencySummary.pop_back();
    }

    if (getRound() == 99) {
        // These operations may need synchronization if Logger is accessed
        // concurrently
        LogWriter::getTestLog()["Frequency"].push_back(frequencySummary);

        // Safe read access to longestChain
        size_t chainSize;
        {
            shared_lock<shared_mutex> chainReadLock(chainMutex);
            chainSize = longestChain.size();
        }

        LogWriter::getTestLog()["Block Chain Length"].push_back(chainSize);
        LogWriter::getTestLog()["Network Total Flipped Blocks"].push_back(
            totalNetworkFlippedBlocks
        );
    }
}
void EthanBitPeer::checkIncomingMessages() {
    // Get messages one at a time instead of trying to get the whole queue at
    // once
    while (!inStreamEmpty()) {
        // Process one message at a time
        Packet<bitcoinMessage> newMsg = popInStream();

        if (newMsg.getMessage().mined) {
            // Add to unlinked blocks with lock
            {
                std::lock_guard<mutex> unlinkedLock(unlinkedBlocksMutex);
                unlinkedBlocks.push_back(newMsg.getMessage().block);
            }
            // Process blocks
            linkBlocks();
        } else {
            // Add transaction with lock
            std::lock_guard<mutex> transLock(transactionsMutex);
            transactions.push_back(newMsg.getMessage().block);
        }
    }
}

bool EthanBitPeer::checkSubmitTrans() { return randMod(submitRate) == 0; }

void EthanBitPeer::submitTrans() {
    const lock_guard<mutex> lock(currentTransaction_mutex);
    bitcoinMessage msg;
    msg.mined = false;
    msg.block.transaction.id = currentTransaction++;
    msg.block.transaction.roundSubmitted = getRound();
    broadcast(msg);
}

bool EthanBitPeer::checkMineBlock() { return randMod(mineRate) == 0; }

void EthanBitPeer::mineBlocks() {
    // Check if there are transactions to mine
    bool hasTrans;
    {
        std::lock_guard<mutex> transLock(transactionsMutex);
        hasTrans = !transactions.empty();
    }

    if (!hasTrans)
        return;

    // Shared lock for reading the chain
    bool emptyChain;
    {
        shared_lock<shared_mutex> chainReadLock(chainMutex);
        emptyChain = longestChain.empty();
    }

    // If our longest chain is empty, create & broadcast a genesis block
    if (emptyChain) {
        bitcoinBlock genesisBlock;
        genesisBlock.blockID = blockCounter++;
        genesisBlock.parentBlockID = -1; // indicates genesis

        // Lock all related data structures
        {
            std::lock_guard<mutex> blocksLock(blocksMutex);
            std::lock_guard<mutex> tipsLock(tipsMutex);
            std::unique_lock<shared_mutex> chainWriteLock(chainMutex);

            blocks[genesisBlock.blockID] = genesisBlock;
            tips.insert(genesisBlock.blockID);
            longestChain.push_back(genesisBlock.blockID);
        }

        // Broadcast genesis block to help other peers converge
        bitcoinMessage genesisMsg;
        genesisMsg.block = genesisBlock;
        genesisMsg.mined = true;
        broadcast(genesisMsg);
    } else {
        // Create new block
        bitcoinBlock newBlock;

        // Get transaction and parent block ID with proper locking
        {
            shared_lock<shared_mutex> chainReadLock(chainMutex);
            newBlock.parentBlockID = longestChain.back();
        }

        newBlock.blockID = blockCounter++;

        // Get and remove a transaction
        {
            std::lock_guard<mutex> transLock(transactionsMutex);
            newBlock.transaction = transactions.front().transaction;
            transactions.erase(transactions.begin());
        }

        // Update blockchain data structures
        {
            std::lock_guard<mutex> blocksLock(blocksMutex);
            std::lock_guard<mutex> edgesLock(edgesMutex);
            std::lock_guard<mutex> tipsLock(tipsMutex);

            blocks[newBlock.blockID] = newBlock;
            edges[newBlock.parentBlockID].push_back(newBlock.blockID);

            // Update tips: remove the parent (since it's no longer a tip) and
            // add the new block
            tips.erase(newBlock.parentBlockID);
            tips.insert(newBlock.blockID);
        }

        // Broadcast the new block
        bitcoinMessage msg;
        msg.block = newBlock;
        msg.mined = true;
        broadcast(msg);
    }
}

bitcoinBlock EthanBitPeer::findNextTransaction() {
    std::lock_guard<mutex> transLock(transactionsMutex);

    if (transactions.empty()) {
        return bitcoinBlock(
        ); // Return an empty block if there are no transactions
    }

    // Get the next block from the transaction list
    bitcoinBlock nextBlock = transactions.back(); // Directly access the block
    transactions.pop_back(); // Remove the block from the list

    // Create a new block and assign necessary information
    nextBlock.blockID = currentTransaction++; // Assign a new block ID
    nextBlock.parentBlockID = getLongestChainTip(
    ); // Set parent block ID to the latest block in the chain

    return nextBlock;
}

void EthanBitPeer::updateLongestChain() {
    vector<int> newLongestChain;
    unordered_map<int, vector<int>> chainPaths;

    // Create local copies of shared data to minimize lock time
    std::unordered_set<int> localTips;
    std::unordered_map<int, bitcoinBlock> localBlocks;

    {
        std::lock_guard<mutex> tipsLock(tipsMutex);
        // Create a copy constructor instead of assignment
        localTips = std::unordered_set<int>(tips.begin(), tips.end());
    }

    {
        std::lock_guard<mutex> blocksLock(blocksMutex);
        localBlocks = blocks; // This should work fine as an assignment
    }

    // Traverse back from each tip to find the longest chain
    for (int tip : localTips) {
        vector<int> currentPath;
        int currentBlockID = tip;

        while (currentBlockID != -1) {
            currentPath.push_back(currentBlockID);
            auto it = localBlocks.find(currentBlockID);
            if (it != localBlocks.end()) {
                currentBlockID = it->second.parentBlockID;
            } else {
                break;
            }
        }

        reverse(currentPath.begin(), currentPath.end());
        chainPaths[tip] = currentPath;

        // Keep the longest chain found
        if (currentPath.size() > newLongestChain.size()) {
            newLongestChain = currentPath;
        }
    }

    // Get current chain for comparison
    vector<int> oldChain;
    {
        shared_lock<shared_mutex> chainReadLock(chainMutex);
        oldChain = longestChain;
    }

    // Check if the chain has changed and update stats
    if (newLongestChain != oldChain) {
        trackChainSwitch(oldChain, newLongestChain);

        // Update the longest chain with a write lock
        {
            std::unique_lock<shared_mutex> chainWriteLock(chainMutex);
            longestChain = newLongestChain;
            longestChainLength = longestChain.size();
        }

        // Update miner stats
        std::lock_guard<mutex> statsLock(minerStatsMutex);
        auto &stats = minerStats[id()];
        std::lock_guard<mutex> chainLock(stats.chainMutex);
        stats.currentChain = newLongestChain;
    }
}

int EthanBitPeer::countFlippedBlocks(
    const vector<int> &oldChain, const vector<int> &newChain
) {
    size_t i = 0;
    // Find where the chains diverge
    while (i < oldChain.size() && i < newChain.size() &&
           oldChain[i] == newChain[i]) {
        i++;
    }

    // Only count the blocks that were abandoned from the old chain
    return oldChain.size() - i;
}

void EthanBitPeer::trackChainSwitch(
    const vector<int> &oldChain, const vector<int> &newChain
) {
    if (oldChain.empty() || newChain.empty()) {
        return;
    }

    int flippedBlocks = countFlippedBlocks(oldChain, newChain);

    if (flippedBlocks > 0) {
        // Lock to safely update miner stats
        std::lock_guard<mutex> statsLock(minerStatsMutex);
        auto &stats = minerStats[id()];

        // Atomic operations don't need locks
        stats.totalSwitches++;
        stats.totalFlippedBlocks += flippedBlocks;

        // Lock for updating flip frequency map
        std::lock_guard<mutex> chainLock(stats.chainMutex);
        ++stats.flipFrequency[flippedBlocks];
    }
}

int EthanBitPeer::getLongestChainTip() const {
    // Lock for reading the chain
    shared_lock<shared_mutex> chainReadLock(chainMutex);
    return longestChain.empty() ? -1 : longestChain.back();
}

int EthanBitPeer::getTotalSwitches(long minerId) const {
    std::lock_guard<mutex> statsLock(minerStatsMutex);
    auto it = minerStats.find(minerId);
    return (it != minerStats.end()) ? it->second.totalSwitches.load() : 0;
}

int EthanBitPeer::getTotalFlippedBlocks(long minerId) const {
    std::lock_guard<mutex> statsLock(minerStatsMutex);
    auto it = minerStats.find(minerId);
    return (it != minerStats.end()) ? it->second.totalFlippedBlocks.load() : 0;
}

const vector<int> &EthanBitPeer::getCurrentChain(long minerId) const {
    static const vector<int> emptyChain;

    std::lock_guard<mutex> statsLock(minerStatsMutex);
    auto it = minerStats.find(minerId);

    if (it != minerStats.end()) {
        std::lock_guard<mutex> chainLock(it->second.chainMutex);
        return it->second.currentChain;
    }

    return emptyChain;
}

ostream &EthanBitPeer::printTo(ostream &os) const {
    size_t chainSize;
    {
        shared_lock<shared_mutex> chainReadLock(chainMutex);
        chainSize = longestChain.size();
    }

    os << "Peer ID: " << id() << " | Blocks: " << chainSize;
    return os;
}

ostream &operator<<(ostream &os, const EthanBitPeer &peer) {
    return peer.printTo(os);
}

void EthanBitPeer::printBlockChain() const {
    cout << "\nFinal Longest Chain For Peer " << id() << ":\n";

    // Lock for reading the chain
    shared_lock<shared_mutex> chainReadLock(chainMutex);

    for (int blockId : longestChain) {
        cout << blockId;
        if (blockId != longestChain.back()) {
            cout << " -> ";
        }
    }
    cout << "\nChain Length: " << longestChain.size() << endl;
}

void EthanBitPeer::printFrequencyData() const {
    std::unordered_map<int, int> globalFlipFrequency;

    {
        std::lock_guard<mutex> statsLock(minerStatsMutex);

        for (const auto &pair : minerStats) {
            std::lock_guard<mutex> chainLock(pair.second.chainMutex);
            const auto &localFrequency = pair.second.flipFrequency;
            for (const auto &entry : localFrequency) {
                int blocksFlipped = entry.first;
                int frequency = entry.second;
                globalFlipFrequency[blocksFlipped] += frequency;
            }
        }
    }

    std::vector<std::pair<int, int>> sortedFrequency(
        globalFlipFrequency.begin(), globalFlipFrequency.end()
    );
    std::sort(
        sortedFrequency.begin(), sortedFrequency.end(),
        [](const auto &a, const auto &b) { return a.first < b.first; }
    );

    int totalNetworkFlippedBlocks = 0;

    {
        std::lock_guard<mutex> statsLock(minerStatsMutex);
        // Cast peers to EthanBitPeer to access their statistics
        for (const auto &pair : minerStats) {
            totalNetworkFlippedBlocks += pair.second.totalFlippedBlocks.load();
        }
    }

    std::string frequencySummary;
    // Number of Blocks Flipped
    for (const auto &entry : sortedFrequency) {
        frequencySummary +=
            std::to_string(entry.first) +
            (entry.first == 1 ? " block flipped: " : " blocks flipped: ") +
            std::to_string(entry.second) + " times\n";
    }

    cout << "\n*****************************" << "\n";
    cout << "\nFrequency Data (Global): " << "\n";
    cout << frequencySummary;
}

Simulation<quantas::bitcoinMessage, quantas::EthanBitPeer> *generateSim() {
    Simulation<quantas::bitcoinMessage, quantas::EthanBitPeer> *sim =
        new Simulation<quantas::bitcoinMessage, quantas::EthanBitPeer>;
    return sim;
}

} // namespace quantas