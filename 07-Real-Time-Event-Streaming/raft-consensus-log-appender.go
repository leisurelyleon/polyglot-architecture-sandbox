package raftconsensus

import (
	"fmt"
	"math/rand"
	"sync"
	"time"
)

// 1. Defining the core state types of a Raft Node
type NodeState int

const (
	Follower NodeState = iota
	Candidate
	Leader
)

// 2. The Raft Server Node
type RaftNode struct {
	mu          sync.Mutex
	id          int
	state       NodeState
	currentTerm int
	votedFor    int
	
	// Channels for inter-goroutine signaling
	heartbeatChan chan bool
	shutdownChan  chan bool
}

func NewRaftNode(id int) *RaftNode {
	return &RaftNode{
		id:            id,
		state:         Follower,
		currentTerm:   0,
		votedFor:      -1,
		heartbeatChan: make(chan bool),
		shutdownChan:  make(chan bool),
	}
}

// 3. The Core Execution Loop (Runs infinitely in a background Goroutine)
func (rn *RaftNode) RunElectionTimer() {
	// Raft requires a randomized timeout to prevent split votes
	timeoutDuration := time.Duration(rand.Intn(150)+150) * time.Millisecond
	timer := time.NewTimer(timeoutDuration)

	for {
		select {
		case <-rn.shutdownChan:
			timer.Stop()
			return

		case <-rn.heartbeatChan:
			// We received a heartbeat from the Leader!
			// Reset the election timer so we remain a obedient Follower
			if !timer.Stop() {
				// Drain the channel if it already fired
				select {
				case <-timer.C:
				default:
				}
			}
			timer.Reset(time.Duration(rand.Intn(150)+150) * time.Millisecond)

		case <-timer.C:
			// The timer expired! The Leader is dead. We must trigger an election.
			rn.mu.Lock()
			if rn.state != Leader {
				rn.becomeCandidate()
				rn.startElection()
			}
			rn.mu.Unlock()

			// Reset timer for the next cycle
			timer.Reset(time.Duration(rand.Intn(150)+150) * time.Millisecond)
		}
	}
}

// 4. State Transitions
func (rn *RaftNode) becomeCandidate() {
	rn.state = Candidate
	rn.currentTerm++
	rn.votedFor = rn.id // Vote for ourselves
	fmt.Printf("[Node %d] Leader timeout. Became Candidate for Term %d\n", rn.id, rn.currentTerm)
}

func (rn *RaftNode) startElection() {
	// In a real system, this sends concurrent RequestVote RPCs to all other nodes.
	// If it receives a majority of votes, it calls becomeLeader().
	fmt.Printf("[Node %d] Broadcasting RequestVote RPCs to peers...\n", rn.id)
}

// 5. The Leader's Log Replication Mechanism
// If this node becomes the Leader, it must constantly broadcast heartbeats to suppress elections
func (rn *RaftNode) BroadcastAppendEntries() {
	ticker := time.NewTicker(50 * time.Millisecond) // Heartbeat interval MUST be faster than the timeout
	defer ticker.Stop()

	for {
		select {
		case <-rn.shutdownChan:
			return
		case <-ticker.C:
			rn.mu.Lock()
			if rn.state == Leader {
				// In a real system, this sends AppendEntries RPCs carrying the event logs to all followers
				fmt.Printf("[Node %d] Broadcasting AppendEntries (Heartbeat) for Term %d\n", rn.id, rn.currentTerm)
			} else {
				rn.mu.Unlock()
				return // We got demoted, stop broadcasting
			}
			rn.mu.Unlock()
		}
	}
}

// 6. External RPC Handler: Receiving a Heartbeat
func (rn *RaftNode) ReceiveAppendEntriesRPC(term int, leaderId int) {
	rn.mu.Lock()
	defer rn.mu.Unlock()

	if term >= rn.currentTerm {
		// We recognize the new leader's authority
		rn.currentTerm = term
		rn.state = Follower
		rn.votedFor = -1
		
		// Signal our timer loop to reset via a non-blocking channel send
		select {
		case rn.heartbeatChan <- true:
		default:
		}
	}
}
