/*
  Big Puzzle - Curious Levity

  The following code adapts the game Puzzle101, to a larger format

  1. Puzzle101 style puzzle for any number of Blinks (user locks in puzzle)
  2. Solution state celebration (recognize global solution and celebrate)
  3. Communicate secret code through flashing
  4. BONUS: Multiclick to enter toy-state
  
  Designed with Paul Levy & Brandon Bozzi
  Code by: Jonathan Bobrow & Ivan Ivanov
  2025.10.30
*/

// HANDICAP FOR THE PUZZLE IS SHOWING WHETHER A CONNECTION IS CORRECT
// MODE 0 - NO HANDICAP (SAME COLORS WILL PULSE TOGETHER, DOESN'T MEAN IT IS THE CORRECT POSITION, THAT IS IT)
// MODE 1 - A LITTLE HANDICAP (WHEN A TILE HAS ALL OF ITS CONNECTIONS CORRECT, IT TURNS FACES GREEN)
// MODE 2 - A LOT OF HANDICAP (WHEN A FACE HAS ITS CORRECT CONNECTION, IT TURNS GREEN) 
#define HANDICAP_MODE 1

#define NUM_GAME_COLORS 4
#define CODE_LENGTH 4 // MAKE SURE THIS MATCHES THE NUMBER OF DIGITS IN THE SECRET CODE
byte secretCode[CODE_LENGTH] = {8, 6, 7, 5};  // REPLACE WITH SECRET CODE

#define TANGERINE makeColorHSB(22,255,255)
#define LEMON     makeColorHSB(49,255,255)
#define MINT      makeColorHSB(99,255,255)
#define GRAPE     makeColorHSB(200,255,255) 
#define LIME      makeColorHSB(82,255,255)
#define BLUEBERRY makeColorHSB(160,255,255)
#define NO_COLOR  makeColorRGB(0,0,0)

Color digitColors[CODE_LENGTH] = {TANGERINE, LEMON, MINT, GRAPE};  // ARRAY OF COLORS FOR EACH DIGIT POSITION
Color gameColors[NUM_GAME_COLORS+1] = {NO_COLOR, TANGERINE, LEMON, MINT, GRAPE};  // ARRAY OF COLORS FOR PUZZLE

#define COMM_INERT          0
#define COMM_SETUP_GO       1
#define COMM_SETUP_RESOLVE  2
#define COMM_PLAY_GO        3
#define COMM_PLAY_RESOLVE   4
#define COMM_PLAY_BASE      5  // PLAY mode uses values 5-20 (base + distance)
#define COMM_WIN_GO         21
#define COMM_WIN_RESOLVE    22

#define MAX_SOLVE_DISTANCE  15  // Maximum distance we can represent

enum State {
  INERT,
  GO,
  RESOLVE
};

enum NegotiationState {
  NEG_INERT,
  NEG_SENT,      // We sent our proposal, waiting for theirs
  NEG_RECEIVED,  // We got their proposal, negotiation complete
  NEG_COMPLETE   // Both sides have exchanged and agreed
};

enum SignatureState {
  SIG_NONE,      // Haven't started signature exchange
  SIG_SENT,      // We sent our signature
  SIG_RECEIVED   // We received their signature (complete)
};

enum Mode {
  SETUP,
  PLAY,
  WIN
};

byte signalState = INERT;
byte gameMode = SETUP;

byte broadcastValue = COMM_INERT;
byte inverseDistanceFromUnsolved = 0;  // Distance propagation for win detection

byte currentNeighbors[6][7] = {{},{},{},{},{},{}};
byte solutionNeighbors[6][7] = {{},{},{},{},{},{}};
byte myFaceColors[6] = {};

#define PKG_NEGOTIATE_COLOR 0
#define PKG_COLOR_SIGNATURE 1

#define MAX_NEG_NUM 255
byte negotiationState[6] = {};
byte myNumbers[6] = {};
byte myProposedColors[6] = {}; // Store proposed color separately
byte signatureState[6] = {};   // Track signature exchange state per face

// SYNCHRONIZED CELEBRATION
Timer syncTimer;
#define PERIOD_DURATION 2000
#define BUFFER_DURATION 200
byte neighborSyncState[6];
byte syncVal = 0;
bool bReadyToSolve = false;

void setup() {
  randomize(); // initialize random numbers w/ entropy
}

void loop() {

  // process incoming packages
  processIncomingPackages();

  // keep clocks in sync
  syncLoop();

  // check our surroundings for state changes
  switch(signalState) {
    
    case INERT:
      inertLoop(); 
      break;

    case GO:
      goLoop(); 
      break;

    case RESOLVE:
      resolveLoop(); 
      break;

  }

    // do the thing our mode wants us to do :)
  switch(gameMode) {
    
    case SETUP:
      setupLoop();
      break;

    case PLAY:
      playLoop();
      break;
    
    case WIN:
      winLoop();
      break;
  }

  // listen for reset clicks (5, no more, no less)
  if(buttonMultiClicked()) {
    if(buttonClickCount() == 5) {
      broadcastValue = COMM_SETUP_GO;
    }
  }
  
  // tell everyone what's up
  byte sendData;
  
  // Check if we're trying to change modes (GO signals)
  bool changingModes = (broadcastValue == COMM_SETUP_GO || 
                        broadcastValue == COMM_PLAY_GO || 
                        broadcastValue == COMM_WIN_GO ||
                        broadcastValue == COMM_SETUP_RESOLVE ||
                        broadcastValue == COMM_PLAY_RESOLVE ||
                        broadcastValue == COMM_WIN_RESOLVE);
  
  // Use distance encoding ONLY when in stable PLAY and not changing modes
  if (gameMode == PLAY && !changingModes) {
    // During stable PLAY mode, encode distance from unsolved in the broadcast value
    sendData = (syncVal << 5) + (COMM_PLAY_BASE + inverseDistanceFromUnsolved);
  } else {
    // Use broadcastValue for mode changes and all other states
    sendData = (syncVal << 5) + (broadcastValue);
  }
  setValueSentOnAllFaces(sendData);

}

void setupLoop() {
  bReadyToSolve = false;  // Reset ready to solve
  
  // Reset signature exchange state when in setup
  FOREACH_FACE(f) {
    signatureState[f] = SIG_NONE;
    
    // IMPORTANT: Clear solution neighbors from previous puzzle
    solutionNeighbors[f][0] = 0;
    solutionNeighbors[f][1] = 0;
    solutionNeighbors[f][2] = 0;
    solutionNeighbors[f][3] = 0;
    solutionNeighbors[f][4] = 0;
    solutionNeighbors[f][5] = 0;
    solutionNeighbors[f][6] = 0;
  }
  
  byte brightness = sin8_C(map(syncTimer.getRemaining(), 0, PERIOD_DURATION, 0, 255))/4;
  if(isAlone()){
    setColor( dim( WHITE, brightness));
  }
  else {
    setColor(OFF);
  }

  // if we see a new neighbor
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { // a neighbor!
      if (currentNeighbors[f][0] == 0) { // color index of 0, no neighbor prior, a new neighbor!
        if(negotiationState[f] == NEG_INERT) {
          byte randNum = random(MAX_NEG_NUM);
          byte randColorIndex = 1 + random(NUM_GAME_COLORS - 1);
          byte negotiationPacket[3] = {PKG_NEGOTIATE_COLOR, randColorIndex, randNum};
          sendDatagramOnFace(negotiationPacket, 3, f);
          myNumbers[f] = randNum;
          myProposedColors[f] = randColorIndex;
          negotiationState[f] = NEG_SENT; // Mark that we sent our proposal
        }
      }
      // Only display color after negotiation is fully complete on both sides
      if(negotiationState[f] == NEG_COMPLETE && myFaceColors[f] != 0) {
        setColorOnFace(dim(gameColors[myFaceColors[f]], 255 - brightness),f);
      }
    }
    else { // no neighor
      myFaceColors[f] = 0;
      myProposedColors[f] = 0;
      currentNeighbors[f][0] = 0;
      negotiationState[f] = NEG_INERT;
    }
  }

  // if user doubleClicks, go to play mode
  if(buttonDoubleClicked()) {
    broadcastValue = COMM_PLAY_GO;
  }
}

void playLoop() {

  // Manual trigger win condition
  if(buttonDoubleClicked()) {
    broadcastValue = COMM_WIN_GO;
  }

  byte brightness = sin8_C(map(syncTimer.getRemaining(), 0, PERIOD_DURATION, 0, 255));
  
  // Check if all signatures have been exchanged
  bool allSignaturesExchanged = true;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { // has a neighbor
      if (signatureState[f] != SIG_RECEIVED) {
        allSignaturesExchanged = false;
        break;
      }
    }
  }
  
  // Show yellow "waiting" state while exchanging signatures
  if (!allSignaturesExchanged) {
    setColor(dim(YELLOW, brightness/2 + 127));
  }
  // Show white "ready" state once all signatures exchanged
  else {
    setColor(dim(WHITE, brightness/2 + 127));
  }

  if(isAlone()) {
    bReadyToSolve = true;
  }
  
  if(bReadyToSolve) {
    
    // Check for new neighbors and send them our state
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) { // has a neighbor
        // If this is a new neighbor (currentNeighbors was cleared), send our info
        if (currentNeighbors[f][0] == 0) {
          byte currentPacket[8] = {
            PKG_COLOR_SIGNATURE,
            myFaceColors[f],
            myFaceColors[0],
            myFaceColors[1],
            myFaceColors[2],
            myFaceColors[3],
            myFaceColors[4],
            myFaceColors[5]
          };
          sendDatagramOnFace(currentPacket, 8, f);
        }
      }
      else { // neighbor disconnected
        // Clear current neighbor data
        currentNeighbors[f][0] = 0;
        currentNeighbors[f][1] = 0;
        currentNeighbors[f][2] = 0;
        currentNeighbors[f][3] = 0;
        currentNeighbors[f][4] = 0;
        currentNeighbors[f][5] = 0;
        currentNeighbors[f][6] = 0;
      }
    }
    
    // === WIN DETECTION using distance propagation ===

    // Check if this Blink is fully solved
    bool amISolved = isAllFacesSolved();

    // Propagate inverse distance from unsolved Blink
    // check neighbors for closest unsolved
    inverseDistanceFromUnsolved = 0;

    if (!amISolved) {
      // I am unsolved, so distance is MAX
      inverseDistanceFromUnsolved = MAX_SOLVE_DISTANCE;
    }

    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborData = getLastValueReceivedOnFace(f);
        
        byte neighborBroadcast = neighborData & 31; // Extract lower 5 bits
        
        // CRITICAL: Only process if neighbor is sending valid distance data
        if (neighborBroadcast >= COMM_PLAY_BASE && neighborBroadcast < COMM_WIN_GO) {
          byte neighborDistance = neighborBroadcast - COMM_PLAY_BASE;
          if (neighborDistance > inverseDistanceFromUnsolved && neighborDistance > 0) {
            inverseDistanceFromUnsolved = neighborDistance - 1;
          }
        }
      }
    }   
    
    // Check for WIN condition: distance is 0 means no unsolved Blinks in the network
    if (inverseDistanceFromUnsolved == 0 && amISolved) {
      // Everyone is solved! Trigger WIN
      // broadcastValue = COMM_WIN_GO;
    }

    // Display colors with matching feedback
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) { // has a neighbor
        // if all faces are solved
        if (amISolved && HANDICAP_MODE == 1) {
          // All faces solved - show GREEN - ONLY FOR EASY MODE 1
          setColorOnFace(dim(GREEN, brightness), f);
          // Color distColor = makeColorHSB(16 * (MAX_SOLVE_DISTANCE - inverseDistanceFromUnsolved), 255, 255);
          // setColorOnFace(distColor, f);
        }
        // Check if this face is fully solved (signature matches solution)
        else if (isFaceSolved(f) && HANDICAP_MODE == 2) {
          // Face solved - show GREEN - ONLY FOR EASY MODE 2
          setColorOnFace(GREEN, f);
        }
        // Check if colors match
        else if (currentNeighbors[f][0] == myFaceColors[f] && currentNeighbors[f][0] != 0) {
          // Colors match but wrong neighbor - pulse
          setColorOnFace(dim(gameColors[myFaceColors[f]], brightness), f);
        }
        else {
          // Colors don't match - solid
          setColorOnFace(gameColors[myFaceColors[f]], f);
        }
      }
      else {
        // No neighbor - show solid color
        setColorOnFace(gameColors[myFaceColors[f]], f);
      }
    }

    // DEBUG - Show what state we're in
    if(broadcastValue == COMM_WIN_GO) {
      setColor(MAGENTA);  // Win detected, transitioning
    }

  }
  
  // Only send signatures after we're fully in PLAY mode (signalState is INERT)
  if (signalState == INERT) {
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) { // has a neighbor
        // Check that neighbor is also in PLAY mode
        byte neighborData = getLastValueReceivedOnFace(f);
        byte neighborMode = getGameMode(neighborData);
        
        if (neighborMode == PLAY && signatureState[f] == SIG_NONE) {
          // Send our signature to this neighbor
          byte signaturePacket[8] = {
            PKG_COLOR_SIGNATURE,
            myFaceColors[f],
            myFaceColors[0],
            myFaceColors[1],
            myFaceColors[2],
            myFaceColors[3],
            myFaceColors[4],
            myFaceColors[5]
          };
          sendDatagramOnFace(signaturePacket, 8, f);
          signatureState[f] = SIG_SENT;
          
          // Store our own color in solution (we know this side)
          // solutionNeighbors[f][0] = myFaceColors[f];
        }
      }
      else { // neighbor disconnected
        signatureState[f] = SIG_NONE;
      }
    }
  }
}

void winLoop() {

  // DEBUG SHOW BLUE IN WIN MODE
  setColor(BLUE);
  // TODO:
  // 1 sec fade to off/black
  // 5 sec fireworks/sparkle
  // 1 second fade sparkle
  // Infinite Code Reveal - blink in first color for first digit the number of times, then switch color and blink for next digit

}

// ===== PACKAGE PROCESSING =====
void processIncomingPackages() {
  FOREACH_FACE(f) {
    if (isDatagramReadyOnFace(f)) {
      const byte* pkg = getDatagramOnFace(f);
      byte pkgType = pkg[0];
      
      switch(pkgType) {

        case PKG_NEGOTIATE_COLOR:
          {
            // Only process color negotiation when in SETUP mode
            if (gameMode != SETUP) {
              break;  // Ignore negotiation packets when not in SETUP
            }
            
            // Also check that neighbor is in SETUP mode
            byte neighborData = getLastValueReceivedOnFace(f);
            byte neighborMode = getGameMode(neighborData);
            if (neighborMode != SETUP) {
              break;  // Ignore if neighbor isn't in SETUP either
            }
            
            // Process negotiation packet if we're in NEG_INERT or NEG_SENT state
            if(negotiationState[f] == NEG_INERT || negotiationState[f] == NEG_SENT) {
              byte neighborColor = pkg[1];
              byte neighborNumber = pkg[2];
              
              // If we haven't sent our proposal yet, send it now
              if(negotiationState[f] == NEG_INERT) {
                byte randNum = random(MAX_NEG_NUM);
                byte randColorIndex = 1 + random(NUM_GAME_COLORS - 1);
                byte negotiationPacket[3] = {PKG_NEGOTIATE_COLOR, randColorIndex, randNum};
                sendDatagramOnFace(negotiationPacket, 3, f);
                myNumbers[f] = randNum;
                myProposedColors[f] = randColorIndex;
              }
              
              // Decide on final color based on who has higher number
              if(neighborNumber > myNumbers[f]) {
                myFaceColors[f] = neighborColor;
              } else if(neighborNumber < myNumbers[f]) {
                myFaceColors[f] = myProposedColors[f];
              } else {
                // Tie-breaker: use lower color index for consistency
                myFaceColors[f] = (myProposedColors[f] < neighborColor) ? myProposedColors[f] : neighborColor;
              }
              
              // Store agreed color
              currentNeighbors[f][0] = myFaceColors[f];
              negotiationState[f] = NEG_RECEIVED;
            }
            // If we already received their packet, mark negotiation complete
            else if(negotiationState[f] == NEG_RECEIVED) {
              negotiationState[f] = NEG_COMPLETE;
            }
          }
          break;

        case PKG_COLOR_SIGNATURE:
          {
            // In PLAY mode: Handle differently based on whether we're setting up or actively playing
            if(gameMode == PLAY) {
              
              // During initial signature exchange (setting up solution)
              if(signatureState[f] != SIG_RECEIVED && !bReadyToSolve) {
                // Store neighbor's color index in SOLUTION
                solutionNeighbors[f][0] = pkg[1];
                // Store neighbor's signature in SOLUTION
                solutionNeighbors[f][1] = pkg[2];
                solutionNeighbors[f][2] = pkg[3];
                solutionNeighbors[f][3] = pkg[4];
                solutionNeighbors[f][4] = pkg[5];
                solutionNeighbors[f][5] = pkg[6];
                solutionNeighbors[f][6] = pkg[7];
                
                // If we haven't sent our signature to this face yet, send it now as a response
                if (signatureState[f] == SIG_NONE) {
                  byte signaturePacket[8] = {
                    PKG_COLOR_SIGNATURE,
                    myFaceColors[f],
                    myFaceColors[0],
                    myFaceColors[1],
                    myFaceColors[2],
                    myFaceColors[3],
                    myFaceColors[4],
                    myFaceColors[5]
                  };
                  sendDatagramOnFace(signaturePacket, 8, f);
                  
                  // Store our own color in solution
                  solutionNeighbors[f][0] = myFaceColors[f];
                }
                
                // Mark that we've received this neighbor's signature
                signatureState[f] = SIG_RECEIVED;
              }
              else {
                // During active play - store in CURRENT neighbors for matching
                currentNeighbors[f][0] = pkg[1];
                currentNeighbors[f][1] = pkg[2];
                currentNeighbors[f][2] = pkg[3];
                currentNeighbors[f][3] = pkg[4];
                currentNeighbors[f][4] = pkg[5];
                currentNeighbors[f][5] = pkg[6];
                currentNeighbors[f][6] = pkg[7];

                // Mark that we've received this neighbor's signature
                signatureState[f] = SIG_RECEIVED;
                
                // If we received their packet but haven't sent ours yet on this reconnection, send now
                if (currentNeighbors[f][0] != 0) {
                  // Check if we need to send our info (we haven't sent since this reconnection)
                  bool needToSend = true;
                  // A simple heuristic: if they have data and we haven't replied, send
                  // We can track this better, but for now just respond to ensure exchange
                  byte currentPacket[8] = {
                    PKG_COLOR_SIGNATURE,
                    myFaceColors[f],
                    myFaceColors[0],
                    myFaceColors[1],
                    myFaceColors[2],
                    myFaceColors[3],
                    myFaceColors[4],
                    myFaceColors[5]
                  };
                  sendDatagramOnFace(currentPacket, 8, f);
                }
              }
            }
            // In SETUP mode: Only process after negotiation is complete
            else if(gameMode == SETUP && negotiationState[f] == NEG_COMPLETE && myFaceColors[f] != 0) {
              // Store neighbor's color index
              currentNeighbors[f][0] = pkg[1];
              // Store neighbor's signature
              currentNeighbors[f][1] = pkg[2];
              currentNeighbors[f][2] = pkg[3];
              currentNeighbors[f][3] = pkg[4];
              currentNeighbors[f][4] = pkg[5];
              currentNeighbors[f][5] = pkg[6];
              currentNeighbors[f][6] = pkg[7];
            }
          }
          break;                    
      }
      
      markDatagramReadOnFace(f);
    }
  }
  
  // Send a second packet to confirm negotiation complete
  FOREACH_FACE(f) {
    if(negotiationState[f] == NEG_RECEIVED) {
      byte negotiationPacket[3] = {PKG_NEGOTIATE_COLOR, myFaceColors[f], myNumbers[f]};
      sendDatagramOnFace(negotiationPacket, 3, f);
      negotiationState[f] = NEG_COMPLETE;
    }
  }
}

// ===== INERT LOOP =====
void inertLoop() {
  //listen for neighbors in GO
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getSignalState(getLastValueReceivedOnFace(f)) == GO) {//a neighbor saying GO!
        signalState = GO;
        gameMode = getGameMode(getLastValueReceivedOnFace(f));
        switch(gameMode) {
          case SETUP: broadcastValue = COMM_SETUP_GO; break;
          case PLAY:  broadcastValue = COMM_PLAY_GO;  break;
          case WIN:   broadcastValue = COMM_WIN_GO;   break;
        }
      }
    }
  }
}

/* ===== GO LOOP =====
 *
 * If all of my neighbors are in GO or RESOLVE, then I can RESOLVE
 *
 */
void goLoop() {  
  signalState = RESOLVE;//I default to this at the start of the loop. Only if I see a problem does this not happen

  switch(gameMode) {
    case SETUP: broadcastValue = COMM_SETUP_RESOLVE; break;
    case PLAY:  broadcastValue = COMM_PLAY_RESOLVE;  break;
    case WIN:   broadcastValue = COMM_WIN_RESOLVE;   break;
  }

  //look for neighbors who have not heard the GO news
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getSignalState(getLastValueReceivedOnFace(f)) == INERT) {//This neighbor doesn't know it's GO time. Stay in GO
        signalState = GO;
        switch(gameMode) {
          case SETUP: broadcastValue = COMM_SETUP_GO; break;
          case PLAY:  broadcastValue = COMM_PLAY_GO;  break;
          case WIN:   broadcastValue = COMM_WIN_GO;   break;
        }
      }
    }
  }
}

/* ===== RESOLVE LOOP =====
 *
 * This loop returns me to inert once everyone around me has RESOLVED
 * Now receive the game mode
 *
 */
void resolveLoop() {
  signalState = INERT;
  broadcastValue = COMM_INERT;

  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      if (getSignalState(getLastValueReceivedOnFace(f)) == GO) {
        signalState = RESOLVE;
        switch(gameMode) {
          case SETUP: broadcastValue = COMM_SETUP_RESOLVE; break;
          case PLAY:  broadcastValue = COMM_PLAY_RESOLVE;  break;
          case WIN:   broadcastValue = COMM_WIN_RESOLVE;   break;
        }
      }
    }
  }
}

/*
 * Keep ourselves on the same time loop as our neighbors
 * if a neighbor passed go, 
 * we want to pass go as well 
 * (if we didn't just pass go)
 * ... or collect $200
 */
void syncLoop() {

  bool didNeighborChange = false;

  // look at our neighbors to determine if one of them passed go (changed value)
  // note: absent neighbors changing to not absent don't count
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) {
      neighborSyncState[f] = 2; // this is an absent neighbor
    }
    else {
      byte data = getLastValueReceivedOnFace(f);
      if (neighborSyncState[f] != 2) {  // wasn't absent
        if (getSyncVal(data) != neighborSyncState[f]) { // passed go (changed value)
          didNeighborChange = true;
        }
      }

      neighborSyncState[f] = getSyncVal(data);  // update our record of state now that we've check it
    }
  }

  // if our neighbor passed go and we haven't done so within the buffer period, catch up and pass go as well
  // if we are due to pass go, i.e. timer expired, do so
  if ( (didNeighborChange && syncTimer.getRemaining() < PERIOD_DURATION - BUFFER_DURATION)
       || syncTimer.isExpired()
     ) {

    syncTimer.set(PERIOD_DURATION); // aim to pass go in the defined duration
    syncVal = !syncVal; // change our value everytime we pass go
  }
}

byte getSyncVal(byte data) {
  return (data >> 5) & 1;
}

byte getSignalState(byte data) {
    byte val = data & 31;
    switch(val) {
      case COMM_INERT:          return INERT;
      case COMM_SETUP_GO:       return GO;
      case COMM_SETUP_RESOLVE:  return RESOLVE;
      case COMM_PLAY_GO:        return GO;
      case COMM_PLAY_RESOLVE:   return RESOLVE;
      case COMM_WIN_GO:         return GO;
      case COMM_WIN_RESOLVE:    return RESOLVE;
      default: {
        // PLAY mode distance values (COMM_PLAY_BASE through COMM_PLAY_BASE + MAX_SOLVE_DISTANCE)
        if (val >= COMM_PLAY_BASE && val < COMM_WIN_GO) {
          return INERT;  // During stable PLAY with distance encoding, we're in INERT state
        }
        return INERT;
      }
  }
}

byte getGameMode(byte data) {
    byte val = data & 31;
    switch(val) {
      case COMM_INERT:          return SETUP;
      case COMM_SETUP_GO:       return SETUP;
      case COMM_SETUP_RESOLVE:  return SETUP;
      case COMM_PLAY_GO:        return PLAY;
      case COMM_PLAY_RESOLVE:   return PLAY;
      case COMM_WIN_GO:         return WIN;
      case COMM_WIN_RESOLVE:    return WIN;
      default: {
        // PLAY mode distance values (COMM_PLAY_BASE through COMM_PLAY_BASE + MAX_SOLVE_DISTANCE)
        if (val >= COMM_PLAY_BASE && val < COMM_WIN_GO) {
          return PLAY;
        }
        return SETUP;
      }
  }
}

// Check if all neighbors signatures match the solution
bool isAllFacesSolved() {
  // check all 6 faces to see if they are solved
  FOREACH_FACE(f) {
    // check facees that should be empty
    if(solutionNeighbors[f][0] == 0) {
      // if it is not empty, it is not solved
      if (!isValueReceivedOnFaceExpired(f)) {
        return false;
      }
      // faces that don't need checking can be skipped
      continue;
    }
    // check faces that should have connections
    if(!isFaceSolved(f)) {
      return false;
    }
  }

  return true;
}

// Check if current neighbor signature matches solution signature for a face
bool isFaceSolved(byte face) {
  // Check if we have both current and solution data
  if (currentNeighbors[face][0] == 0 || solutionNeighbors[face][0] == 0) {
    return false;
  }
  
  // Compare all 7 bytes: color index + 6 signature bytes
  for (byte i = 0; i < 7; i++) {
    if (currentNeighbors[face][i] != solutionNeighbors[face][i]) {
      return false;
    }
  }
  
  return true;
}
