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
#define COMM_WIN_GO         5
#define COMM_WIN_RESOLVE    6

enum State {
  INERT,
  GO,
  RESOLVE
};

enum Mode {
  SETUP,
  PLAY,
  WIN
};

byte signalState = INERT;
byte gameMode = SETUP;

byte broadcastValue = COMM_INERT;

byte currentNeighbors[6][7] = {{},{},{},{},{},{}};
byte solutionNeighbors[6][7] = {{},{},{},{},{},{}};
byte myFaceColors[6] = {};
byte mySignature[6] = {};

#define PKG_NEGOTIATE_COLOR 0
#define PKG_COLOR_SIGNATURE 1

#define MAX_NEG_NUM 255
byte negotiationState[6] = {};
byte myNumbers[6] = {};

// SYNCHRONIZED CELEBRATION
Timer syncTimer;
#define PERIOD_DURATION 2000
#define BUFFER_DURATION 200
byte neighborSyncState[6];
byte syncVal = 0;

void setup() {
  randomize(); // initialize random numbers w/ entropy
}

void loop() {

  // process incoming packages
  processIncomingPackages();

  // keep clocks in sync
  syncLoop();

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
  
  // tell everyone what's up
  byte sendData = (syncVal << 5) + (broadcastValue);
  setValueSentOnAllFaces(sendData);
}

void setupLoop() {
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
        if(negotiationState[f] == INERT) {
          byte randNum = random(MAX_NEG_NUM);
          byte randColorIndex = 1 + random(NUM_GAME_COLORS - 1);
          byte negotiationPacket[3] = {PKG_NEGOTIATE_COLOR, randColorIndex, randNum};
          sendDatagramOnFace(negotiationPacket, 3, f);
          myNumbers[f] = randNum;
          myFaceColors[f] = randColorIndex;
          negotiationState[f] = GO;
        }
      }
      setColorOnFace(dim(gameColors[myFaceColors[f]], 255 - brightness),f);
    }
    else { // no neighor
      myFaceColors[f] = 0;
      currentNeighbors[f][0] = 0;
      negotiationState[f] = INERT;
    }
  }
  // DEBUG SYNC
  // switch(syncVal){
  //   case 0: setColorOnFace(RED, 0); break;
  //   case 1: setColorOnFace(BLUE, 0); break;
  //   case 2: setColorOnFace(GREEN, 0); break;
  // }

  // enter negotiation  
    // negotiate color
    // send neighbor a color and a random number
    // listen for neighbor color and number
      // settle on color with larger shared number
      // end negotiation
  // if negotiation complete, share signature  
    // send them the color on the face they are connected to and our signature

  // listen for a neighbors color and their signature
  // save a neighbors color and signature
  
  // if we lose a neighbor
  // delete our record of our neighbors' color and signature

  // if user doubleClicks, go to play mode
  if(buttonDoubleClicked()) {
    broadcastValue = COMM_PLAY_GO;
  }
}

void playLoop() {
  setColor(WHITE);
  // on entering play mode, lock in neighbors' color and signature to SOLUTION_COLOR_SIGNATURE...
  // show all white, until able to be solved
  // on isAlone() allow engage solveability

  // if we see a new neighbor
  // send them the color on the face they are connected to and our signature
  // listen for a neighbors color and their signature
  // save a neighbors color and signature

  // if we lose a neighbor
  // delete our record of our neighbors' color and signature

  // if neighbors match color, animate color matched

  // if our current neighborhood, matches solve state
  // mark ourselves solved

  // listen for win condition
  if(buttonSingleClicked()) {
    broadcastValue = COMM_WIN_GO;
  }

  // listen for reset clicks (5, no more, no less)
  if(buttonMultiClicked() && buttonClickCount() == 5) {
    broadcastValue = COMM_SETUP_GO;
  }
}

void winLoop() {
  setColor(BLUE);
  // 1 sec fade to off/black
  // 5 sec fireworks/sparkle
  // 1 second fade sparkle
  // Infinite Code Reveal

  // listen for reset clicks (5, no more, no less)
  if(buttonMultiClicked() && buttonClickCount() == 5) {
    broadcastValue = COMM_SETUP_GO;
  }
}

// ===== PACKAGE PROCESSING =====
void processIncomingPackages() {
  FOREACH_FACE(f) {
    if(myFaceColors[f] == 0 || currentNeighbors[f][0] != 0) continue; // both sides aren't ready
    
    if (isDatagramReadyOnFace(f)) {
      const byte* pkg = getDatagramOnFace(f);
      byte pkgType = pkg[0];
      
      switch(pkgType) {

        case PKG_NEGOTIATE_COLOR:
          {
            byte neighborColor = pkg[1];
            byte neighborNumber = pkg[2];
            if(neighborNumber > myNumbers[f]) {
              myFaceColors[f] = neighborColor;
            }
            currentNeighbors[f][0] = myFaceColors[f];
            negotiationState[f] = INERT;
          }
          break;

        case PKG_COLOR_SIGNATURE:
          // Store neighbor's color index
          currentNeighbors[f][0] = pkg[1];
          // Store neighbor's signature
          currentNeighbors[f][1] = pkg[2];
          currentNeighbors[f][2] = pkg[3];
          currentNeighbors[f][3] = pkg[4];
          currentNeighbors[f][4] = pkg[5];
          currentNeighbors[f][5] = pkg[6];
          currentNeighbors[f][6] = pkg[7];
          break;                    
      }
      
      markDatagramReadOnFace(f);
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
  signalState = INERT;//I default to this at the start of the loop. Only if I see a problem does this not happen
  broadcastValue = COMM_INERT;

  //look for neighbors who have not moved to RESOLVE
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getSignalState(getLastValueReceivedOnFace(f)) == GO) {//This neighbor isn't in RESOLVE. Stay in RESOLVE
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
    switch(data & 31) {
      case COMM_INERT:          return INERT;
      case COMM_SETUP_GO:       return GO;
      case COMM_PLAY_GO:        return GO;
      case COMM_WIN_GO:         return GO;
      case COMM_SETUP_RESOLVE:  return RESOLVE;
      case COMM_PLAY_RESOLVE:   return RESOLVE;
      case COMM_WIN_RESOLVE:    return RESOLVE;
  }
}

byte getGameMode(byte data) {
    switch(data & 31) {
      case COMM_INERT:          return SETUP;
      case COMM_SETUP_GO:       return SETUP;
      case COMM_SETUP_RESOLVE:  return SETUP;
      case COMM_PLAY_GO:        return PLAY;
      case COMM_PLAY_RESOLVE:   return PLAY;
      case COMM_WIN_GO:         return WIN;
      case COMM_WIN_RESOLVE:    return WIN;
  }
}