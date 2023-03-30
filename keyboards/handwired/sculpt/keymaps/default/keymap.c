#include <print.h>
#include "debug.h"
#include QMK_KEYBOARD_H

//static bool ambreathing = false;
static uint8_t caps_state = 0;

static uint16_t top = 32;
static uint8_t bottom = 1;
static uint8_t delay = 32;

//static uint16_t extend_dark = 0;
//static uint16_t total_darkness = 40;
static uint32_t ovf_interrupts;
static uint32_t comp_interrupts;
static uint8_t delay_cycles;

/*
static uint16_t breathing_counter = 0;
static uint8_t breath_scale_counter = 1;
const uint8_t   breathing_ISR_frequency     = 120;
const uint16_t   breathing_period    = 6;
static uint16_t breathing_freq_scale_factor = 2;
#    define BREATHING_STEPS 128
#    define BACKLIGHT_LEVELS 10
*/

#    define TOP 32

enum {
    TD_KC_PAUSE_SUDO,
    TD_KC_SLCK_VPN,
};

enum custom_keycodes {
    SUDO = SAFE_RANGE,
    INC_COMP,
    DEC_COMP
};

static bool breathing = false;
static bool led_on = false;
static bool going_up = false;

bool is_breathing(void) { return breathing; }
bool is_going_up(void) { return going_up; }

#        define breathing_interrupt_enable()  \
            do {                              \
                breathing = true;             \
		dprintf("interrupt_enable_called");       \
            } while (0)
#        define breathing_interrupt_disable() \
            do {                              \
                breathing = false;            \
		dprintf("interrupt_disable_called");      \
            } while (0)


#        define led_on()                      \
            do {                              \
                led_on = true;                \
            } while (0)
#        define led_off()                     \
            do {                              \
                led_on = false;               \
            } while (0)


#        define going_up()                    \
            do {                              \
                going_up = true;              \
            } while (0)
#        define going_down()                  \
            do {                              \
                going_up = false;             \
            } while (0)


static inline void enable_pwm(void) {
    //TCCR0A |= _BV(COM0B1) | _BV(COM0B0);
    //TCCR0A |= _BV(COM0A1) | _BV(COM0B1);
}

static inline void disable_pwm(void) {
    //TCCR0A &= ~(_BV(COM0B1) | _BV(COM0B0));
    //TCCR0A &= ~(_BV(COM0A1) | _BV(COM0B1));
    TCCR0A &= ~(_BV(WGM01) | _BV(WGM00));
}

static inline void init_pwm_mode3_int_clock(void) {
    TCCR0A |= (1 << WGM01) | (1 << WGM00); // setting bits 0 and 1 of the TCCR0A register to 1. Waveform Generation Mode
    TCCR0B |= _BV(CS00);                   // setting to internal clock
}

static inline void deinit_pwm_mode3_int_clock(void) {
    TCCR0A &= ~(_BV(WGM01) | _BV(WGM00));
    TCCR0B &= ~(_BV(CS00));
}

void breathing_task(void) {}

void breathing_enable(void) {
    breathing_interrupt_enable();
    going_down();
}

void breathing_disable(void) {
    if(led_on){
	writePin(BACKLIGHT_PIN, 0);
	led_off();
	}    
    breathing_interrupt_disable();
    TCNT0 = 0;
    OCR0B = 0;
    disable_pwm();
    ovf_interrupts =0;
    comp_interrupts =0;
    delay_cycles=0;
    breathing = false;
    led_on = false;
    going_up = false;

    TIMSK0 &= ~(1 << OCIE0B);
    TIFR0 &= ~(1 << OCF0B);
}

void breathing_toggle(void) {
    if (is_breathing())
        breathing_disable();
    else
        breathing_enable();
}

/* To generate breathing curve in python:
 * from math import sin, pi; [int(sin(x/128.0*pi)**4*255) for x in range(128)]
 */
//static const uint8_t breathing_table[BREATHING_STEPS] PROGMEM = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 17, 20, 24, 28, 32, 36, 41, 46, 51, 57, 63, 70, 76, 83, 91, 98, 106, 113, 121, 129, 138, 146, 154, 162, 170, 178, 185, 193, 200, 207, 213, 220, 225, 231, 235, 240, 244, 247, 250, 252, 253, 254, 255, 254, 253, 252, 250, 247, 244, 240, 235, 231, 225, 220, 213, 207, 200, 193, 185, 178, 170, 162, 154, 146, 138, 129, 121, 113, 106, 98, 91, 83, 76, 70, 63, 57, 51, 46, 41, 36, 32, 28, 24, 20, 17, 15, 12, 10, 8, 6, 5, 4, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Use this before the cie_lightness function.
//static inline uint16_t scale_backlight(uint16_t v) { return v / BACKLIGHT_LEVELS * get_backlight_level(); }
ISR(TIMER0_COMPB_vect)
{

	//OCR0B = comp + TCNT0;
	//comp_interrupts++;
	//if (comp_interrupts > 10000){
	//	dprintf("10000 comp interrups \n");
	//	comp_interrupts=0;
	//}


#ifdef MYLEDTIMER

   delay_cycles++;
   ovf_interrupts++;

   if (is_breathing() && !led_on && ovf_interrupts > top){      // off 3 sec = 3000
        writePin(BACKLIGHT_PIN, 1);
        led_on();
        ovf_interrupts = 0;
    }
   
else if (is_breathing() && led_on && ovf_interrupts >= bottom){  // on 1 sec = 1000
                writePin(BACKLIGHT_PIN, 0);
                led_off();
                ovf_interrupts = 0;
        }

   if (delay_cycles > delay){
        if (!is_going_up()){  // going down
           if (top >= 1){
                top--;
                }
           else{            // top is 0 here, switching direction to up
                top = 1;
                going_up();
                top++;
           }
        }
        else{                   // going up
           if (top <= TOP){
                top++;
                }
           else{                  // botom 0 here
                top = TOP;
                bottom = 1;
                going_down();
                top--;
           }
        }
        delay_cycles = 0;
        }

#endif
}

ISR(TIMER0_OVF_vect)
{
#ifdef MYLEDTIMER
//Timer overflow 
 
         //OCR0B += 100;
        //OCR0A += 100;
/*
        comp_interrupts++;
        if (comp_interrupts > 10000){
                dprintf("10000 comp interrups \n");
                comp_interrupts=0;
        }
*/
#endif
}



void led_pwm_test(void){  
#ifdef MYLEDTIMER
         breathing_interrupt_enable();
	 init_pwm_mode3_int_clock();
	/*	Modes controlled by 3 bits WGM02 (in TCCR0B register!),  and WGM01,  WGM00 in TCCR0A register 
		Mode	WGM02	WGM01	WGM00	Mode of Operation	TOP	Update of OCRx at	TOV flag set on
		----	----	----	-----	-------------------	----	----------------------	---------------
		0	0	0	0	Normal			0xFF	Immediate		MAX	
		1	0	0	1	PWM, phase correct	0xFF	TOP			BOTTOM
		2	0	1	0	CTC			OCRA	Immediate		MAX
		3	0	1	1	Fast PWM		0xFF	TOP			MAX
		4	1	0	0	Reserved		-	-			-
		5	1	0	1	PWM, phase correct	OCRA	TOP			BOTTOM
		6	1	1	0	Reserved		-	-			-	
		7	1	1	1	Fast PWM		OCRA	TOP			TOP
	
	*/

	//breathing_interrupt_enable();
	enable_pwm();

        /* reset all the timers and comparators */
        //OCR0A = 0xFF;
        OCR0B = 254;
        TCNT0 = 0;
	//TIMSK0 |= (1 << OCR0B);
	TIMSK0 |= (1 << OCIE0B);
	TIMSK0 |= (1 << TOV0);
	//TIFR0  |= (1 << OCF0B);
	//TIMSK0 = 1 << TOIE0;
	sei();			    // enable global interrupts - not necessary, works without it ok
       // DDRB |= (1 << DDB7);        // B7 Pin

#endif
}


bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
    case SUDO:
        if (record->event.pressed) {
            // when keycode SUDO is pressed 
            SEND_STRING("-------------");
        } else {
            // when keycode QMKBEST is released
        }
        break;
    case INC_COMP:
	if (record->event.pressed) {
        } else {
            // when keycode TEST_LED is released
        }
	break;
    case DEC_COMP:
        if (record->event.pressed) {
	} else {
            // when keycode TEST_LED is released
        }
	break;
    }
    return true;
};

void dance_egg_sudo(tap_dance_state_t *state, void *user_data) {
    if (state->count >= 2) {
	//sends sudo on double or more clicks followed by sending enter 
        SEND_STRING("********");
        wait_ms(1000);
	SEND_STRING("*********");
        reset_tap_dance(state);
    }
}

void dance_egg_vpn(tap_dance_state_t *state, void *user_data) {
    if (state->count >= 2) {
        //sends sudo on double or more clicks followed by sending enter
        SEND_STRING(".......");
        wait_ms(1000);
	SEND_STRING(".......");
        reset_tap_dance(state);
    }
}


/* When below function returns true bool led_update_kb(led_t led_state) (in led.c) will be run, 
when false - it won't run. So if override is required implement logic in below method and return false */

bool led_update_user(led_t led_state) {
    //#ifdef AUDIO_ENABLE
    //static uint8_t caps_state = 0;
    //if (caps_state != led_state.caps_lock) {
    //    led_state.caps_lock ? PLAY_SONG(caps_on) : PLAY_SONG(caps_off);
    //    caps_state = led_state.caps_lock;
    //}
    //#endif

/*
    #ifdef BACKLIGHT_BREATHING
        if (led_state.caps_lock == 1)
                {
                        breathing_enable();
                }
        else
                {
                        breathing_disable();
                }
    #endif
*/
//    #ifdef MY_CAPS_LOCK_PIN
//           writePin(MY_CAPS_LOCK_PIN, led_state.caps_lock);
//    #endif

   //#ifdef LED_CAPS_LOCK_PIN
   //     writePin(LED_CAPS_LOCK_PIN, led_state.caps_lock);
   //#endif	

    if (caps_state != led_state.caps_lock) {
	//backlight_level(10);
        //led_state.caps_lock ? backlight_enable() : backlight_disable();
        led_state.caps_lock ? led_pwm_test() : breathing_disable();
        caps_state = led_state.caps_lock;
    }
   return false;
}


// Tap Dance definitions
tap_dance_action_t tap_dance_actions[] = {
    // Tap once for R SHIFT, twice for Sudo
    //[TD_RSHIFT_SUDO] = ACTION_TAP_DANCE_FN(dance_egg_sudo)
    [TD_KC_PAUSE_SUDO] = ACTION_TAP_DANCE_FN(dance_egg_sudo),
    [TD_KC_SLCK_VPN] = ACTION_TAP_DANCE_FN(dance_egg_vpn)
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
/*
*
* |ESC | F1 | F2 | F3 | F4 | F5 | F6 | f7 | F8 | F9 | F10| F11| F12|PrtS|ScrL|Paus|Calc|
*  -------------------------------------------------------------------------------'
* |  ~ |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 |  0 |  - |  = |Bakspace| Del |Home|
* --------------------------------------------------------------------------
* | tab  |  q |  w |  e |  r |  t |  y |  u |  i |  o |  p |  [ |  ] |  \   |     |End |
*  -------------------------------------------------------------------------------'
* | caps  |  a |  s |  d |  f |  g |  h |  j |  k |  l |  ; |  ' | enter     |Inst|PgUp|
* --------------------------------------------------------------------------------
* |Lsft    |  z |  x |  c |  v |  b |  n |  m |  , |  . |  / |      Rsft| Up| PgDn|
* ---------------------------------------------------------------------------------
* |Lctl   |Lgui  |Lalt |       Spc1 | Spc2    |Ralt  | Menu|  Rctl |Left|Down|Rght|
* ---------------------------------------------------------------------------------
*/

  [0] = LAYOUT( \
    KC_ESC,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,     KC_F12,  KC_PSCR, TD(TD_KC_SLCK_VPN), TD(TD_KC_PAUSE_SUDO), KC_CALC, \
    KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS,    KC_EQL,           KC_BSPC, KC_DEL, KC_HOME,  \
    KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC,    KC_RBRC,          KC_BSLS, KC_END, \
    KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,                      KC_ENT,  KC_INS, KC_PGUP, \
    KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,                      	     KC_RSFT, KC_UP,   KC_PGDN, \
    KC_LCTL, KC_LGUI, KC_LALT,                            KC_SPC,  SCULPT_RIGHT_SPACE_KEY,           KC_RALT, KC_APP, KC_RCTL, KC_LEFT, KC_DOWN, KC_RGHT  \
  ),
  [1] = LAYOUT( \
    _______, INC_COMP, DEC_COMP, _______, _______, KC_WWW_SEARCH, KC_WWW_REFRESH, _______, _______, KC_MEDIA_PLAY_PAUSE, KC_MUTE, KC_VOLD, KC_VOLU, _______, _______, _______, \
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    _______, _______, _______, _______, _______, \
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,    _______,          _______, _______, \
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                      _______, _______, _______, \
    _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,                      _______, _______, _______, \
    _______, _______, _______,                            _______, _______,                         _______, _______, _______, _______, _______, _______ \
  )
};
