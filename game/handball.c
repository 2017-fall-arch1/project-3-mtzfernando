#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>

#define GREEN_LED BIT6

int paddlePos = 0;
int screen = 0;
int button;

AbRect paddleRect = {abRectGetBounds, abRectCheck, {20, 3}}; /**< 10x10 rectangle */

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 1, screenHeight/2 - 1}
};

Layer home = {
  (AbShape *) &fieldOutline,
  {screenWidth / 2, screenHeight / 2},
  {0, 0}, {0, 0},
  COLOR_ORANGE,
  0
};

Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  0
};

Layer paddle = {
  (AbShape *)&paddleRect,
  {(screenWidth/2), 156},
  {0,0}, {0,0},
  COLOR_RED,
  &fieldLayer
};

Layer ball = {		/**< Layer with an orange circle */
  (AbShape *)&circle2,
  {(screenWidth/2), (screenHeight/2)}, /**< bit below & right of center */
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
	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
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

  if(!(BIT1 & button) || !(BIT2 & button)){
    clearScreen(COLOR_GREEN);
    layerDraw(&ball);
    screen = 1;
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
    else{
      while(!redrawScreen){ /**< Pause CPU if screen doesn't need updating */
	P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
	or_sr(0x10);	      /**< CPU OFF */
      }
      P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
      redrawScreen = 0;
      movLayerDraw(&movBall, &ball);
      movePaddle(button);
    }
  }
}

/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler(){
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if(count == 20){
    mlAdvance(&movBall, &fieldFence);
    if(p2sw_read())
      redrawScreen = 1;
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
