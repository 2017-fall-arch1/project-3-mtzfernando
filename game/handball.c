/*References:
 *           Dr. Freudenthal code
*/
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>

#define GREEN_LED BIT6

int paddlePos = 0;        //For the position of the paddle.
int screen = 0;           //For the screen to display.
int button;               //For the buttons.
int gameOverFlag = 0;     //For clearing the screen only once.
char score = '0';         //For the score of the game.

AbRect paddleRect = {abRectGetBounds, abRectCheck, {20, 3}}; /*20x3 rectangle */

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 1, screenHeight/2 - 1}
};

Layer home = {                /*Layer for the main screen*/
  (AbShape *) &fieldOutline,
  {screenWidth / 2, screenHeight / 2},
  {0, 0}, {0, 0},
  COLOR_ORANGE,
  0
};

Layer fieldLayer = {		/*playing field as a layer*/
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},          /*center*/
  {0,0}, {0,0},				    /*last & next pos*/
  COLOR_WHITE,
  0
};

Layer paddle = {          /*Layer for the paddle.*/
  (AbShape *)&paddleRect,
  {(screenWidth/2), 156},
  {0,0}, {0,0},
  COLOR_RED,
  &fieldLayer
};

Layer ball = {		/*Layer with an black ball*/
  (AbShape *)&circle2,
  {(screenWidth/2), (screenHeight/2)}, /**Right in the center.*/
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &paddle
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s{
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

/* initial value of {0,0} will be overwritten */
MovLayer movPaddle = {&paddle, {1, 1}, 0};
MovLayer movBall = {&ball, {3, 3}, 0};


void movLayerDraw(MovLayer *movLayers, Layer *layers){
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for(movLayer = movLayers; movLayer; movLayer = movLayer->next){ /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for(movLayer = movLayers; movLayer; movLayer = movLayer->next){ /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for(row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++){
      for(col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++){
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for(probeLayer = layers; probeLayer; probeLayer = probeLayer->next){ /* probe all layers, in order */
	  if(abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)){
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}

/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlAdvance(MovLayer *ml, Region *fence){
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next){
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++){
      if((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) || (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis])){
	fieldLayer.pos.axes[1] += 50;             //For checking the bottom fence.
	//Check if the ball hit the bottom fence.
	if(abRectCheck(&fieldOutline, &(fieldLayer.pos), &(ml->layer->pos)) && axis == 1){
	  screen = 2;
	} else{
	  int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	  newPos.axes[axis] += (2*velocity);
	}
	fieldLayer.pos.axes[1] -= 50;
	//Check if the ball hit the paddle.
	if(abRectCheck(&paddleRect, &(paddle.pos), &(ml->layer->pos)) && axis == 1){
	  int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	  newPos.axes[axis] += (2*velocity);
	  score++;
	  screen = 1;
	}
      }	/**< if outside of fence */
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}

/*Function for the main screen*/
void mainScreen(){
  drawString5x7(40, 3, "HANDBALL", COLOR_BLACK, COLOR_GREEN);
  drawString5x7(47, 30, "PRESS", COLOR_BLACK, COLOR_GREEN);
  drawString5x7(37, 50, "S2 OR S3", COLOR_BLACK, COLOR_GREEN);
  drawString5x7(43, 70, "BUTTON", COLOR_BLACK, COLOR_GREEN);
  drawString5x7(25, 90, "TO START GAME", COLOR_BLACK, COLOR_GREEN);
  //Detect any of the center button to start the game.
  if(!(BIT1 & button) || !(BIT2 & button)){
    clearScreen(COLOR_GREEN);
    layerDraw(&ball);
    screen = 1;
  }
}
/*Function for when the game is over.*/
void gameOver(){
  if(gameOverFlag == 0){       //Only clear the screen one time.
    clearScreen(COLOR_GREEN);
    gameOverFlag++;
  }
  drawString5x7(40, 2, "Game Over", COLOR_BLACK, COLOR_GREEN);
  drawString5x7(43, 17, "SCORE:", COLOR_BLACK, COLOR_GREEN);
  drawString5x7(81, 17, &score, COLOR_BLACK, COLOR_GREEN);
  drawString5x7(41, screenHeight / 2, "PRESS ANY", COLOR_BLACK, COLOR_GREEN);
  drawString5x7(42, (screenHeight / 2) + 15, "BUTTON TO", COLOR_BLACK, COLOR_GREEN);
  drawString5x7(52, (screenHeight / 2) + 30, "RESET", COLOR_BLACK, COLOR_GREEN);
  //Detect if any of the buttons is pressed to reset the game.
  if (!(BIT0 & button) || !(BIT1 & button) || !(BIT2 & button) || !(BIT3 & button)){
      clearScreen(0);
      screen = 0;
      gameOverFlag = 0;
      score = '0';                                 //Reset score.
      ball.posNext.axes[0] = screenWidth / 2;      //Reset the ball's x-coordinate.
      ball.posNext.axes[1] = screenHeight / 2;     //Reset the ball's y-coordinate.
      layerDraw(&home);
    }
}

/*Function to move the paddle.*/
void movePaddle(int button){
  //Move the paddle to the next position.
  movPaddle.layer->posNext.axes[0] = movPaddle.layer->pos.axes[0] + paddlePos;
  if(!(BIT0 & button)){             //Move left when S1 is pressed.
    if(movPaddle.layer->pos.axes[0] >= 25)
      paddlePos = -10;
    else
      paddlePos = 0;
  } else{                            //Move right when S4 is pressed.
    if(!(BIT3 & button)){
    if(movPaddle.layer->pos.axes[0] <= 100)
      paddlePos = 10;
    else
      paddlePos = 0;
  } else
      paddlePos = 0;
  }
  //Redraw the screen.
  movLayerDraw(&movPaddle, &paddle);
}

u_int bgColor = COLOR_GREEN;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */
Region fieldFence;		/**< fence around playing field  */

/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main(){
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  p2sw_init(BIT0 + BIT1 + BIT2 + BIT3);
  clearScreen(COLOR_TAN);
  layerInit(&ball);
  layerDraw(&home);
  layerGetBounds(&fieldLayer, &fieldFence);
  
  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */

  for(;;){
    button  = p2sw_read();
    if(screen == 0)
      mainScreen();
    else if(screen == 2)
      gameOver();
    else{
      while(!redrawScreen){ /**< Pause CPU if screen doesn't need updating */
	P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
	or_sr(0x10);	      /**< CPU OFF */
      }
      P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
      redrawScreen = 0;
      drawString5x7(2, 3, "SCORE:", COLOR_RED, COLOR_GREEN);
      drawString5x7(40, 3, &score, COLOR_RED, COLOR_GREEN);
      movePaddle(button);
      movLayerDraw(&movBall, &ball);
      //Game finished at 9.
      if(score == '9'){
	screen = 2;
      }
    }
  }
}

/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler(){
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if(count == 20){
    if(screen == 1)
      mlAdvance(&movBall, &fieldFence);
    if(p2sw_read())
      redrawScreen = 1;
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
