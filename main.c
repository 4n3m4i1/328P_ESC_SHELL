/*
 * TERM_ESC_SERVO.c
 *
 * Created: 1/7/2023 2:31:04 PM
 * Author : Joseph A De Vico
 */ 
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// HEADER
#define F_CPU	16000000UL
#define BAUD	115200
#define USARTARG    F_CPU/16/BAUD-1


// Timer Alias

#define T2_CTRLA		TCCR2A
#define T2_CTRLB		TCCR2B
#define T2_VAL			TCNT2
#define T2_COMPA		OCR2A
#define T2_COMPB		OCR2B
#define T2_IMASK		TIMSK2
#define T2_IFLAG		TIFR2

#define CLEAR_T1_PRE	TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));	// Clear prescalar
#define SET_T1_PRE_1	TCCR1B |= (1 << CS10);									// Set prescale 1
#define SET_T1_PRE_8	TCCR1B |= (1 << CS11);									// Set prescale 8
							
#define T2_STOP_TIMER		TCCR2B = 0x00;
#define T2_EN_COMPA_ISR		TIMSK2 |= (1 << OCIE2A);
#define T2_NE_COMPA_ISR		TIMSK2 &= ~(1 << OCIE2A);

// Setup CTC Mode 1s interval
#define T2_10MS_SETUP		TCCR2A = (1 << WGM21);	\
							TCCR2B = 0x00;			\
							TCNT2  = 0x00;			\
							OCR2A = 125;			\
							TIFR2 |= (1 << TOV2);	\
							T2_EN_COMPA_ISR			\
							WAIT_FLAG_T2 = 1;		\
							TCCR2B = ((1 << CS22) | (1 << CS21) | (1 << CS20));

#define T2_1MS_SETUP		TCCR2A = (1 << WGM21);	\
							TCCR2B = 0x00;			\
							TCNT2  = 0x00;			\
							OCR2A  = 249;			\
							TIFR2 |= (1 << TOV2);	\
							T2_EN_COMPA_ISR			\

#define T2_BEGIN_1MS		TCCR2B = 0x00;			\
							TCNT2 = 0x00;			\
							WAIT_FLAG_T2 = 1;		\
							TCCR2B = (1 << CS21) | (1 << CS20);

#define T2_1MS_SETUP_ENC	TCCR2A = (1 << WGM21);	\
							TCCR2B = 0x00;			\
							TCNT2  = 0x00;			\
							OCR2A = 249;			\
							TIFR2 |= (1 << TOV2);	\
							T2_EN_COMPA_ISR			\
							WAIT_FLAG_T2 = 1;		\
							TCCR2B = ((1 << CS21) | (1 << CS20));

// Strobe toggle for scope triggering
#define TOGGLE_INDIC_STROBE PORTD ^= (1 << PIND7);

// Standard 80 x 24 terminal
#define TERM_W		80
#define TERM_H		24

#define CENTER_W	TERM_W / 2
#define CENTER_H	TERM_H / 2

#define SENDESC   serialWrite(27); serialWrite('[');

/*** Text modes ***/
#define MODE_NONE         '0'
#define MODE_BOLD         '1'
#define MODE_DIM          '2'
#define MODE_UNDERLINE    '4'
#define MODE_BLINK        '5'
#define MODE_REVERSED     '7'
#define MODE_CONCEALED    '8'

/*** Text colours ***/
#define COL_FOREGROUND    '3'
#define COL_BACKGROUND    '4'

#define COL_BLACK         '0'
#define COL_RED           '1'
#define COL_GREEN         '2'
#define COL_YELLOW        '3'
#define COL_BLUE          '4'
#define COL_MAGENTA       '5'
#define COL_CYAN          '6'
#define COL_WHITE         '7'

/*** Cursor move direction ***/
#define MOVE_UP           'A'
#define MOVE_DOWN         'B'
#define MOVE_RIGHT        'C'
#define MOVE_LEFT         'D'


#define NEWLINE		10
#define ENTER_KEY	13
#define CTRL_X		24
#define BACKSPACE	8
#define DELETE		127
#define CTRL_R		18
#define CTRL_A		1

#ifndef	EXASCII
#define GFX_CHAR	'#'
#endif


void init_serial(unsigned int someVal);
void serialWrite(unsigned char data);
uint8_t serialGet();
void serialWriteStr(const char *inpu);
void serialWriteNl(unsigned char data);
void printStr(uint8_t data);
void printBin8(uint8_t data);

void fastBorder(uint8_t clr_scrn);
void printBarFrame(uint8_t top, uint8_t right, uint8_t internalWidth);
void updateBarValue(uint8_t top, uint8_t right, uint8_t internalWidth, uint8_t oldPos, uint8_t newPos);
uint8_t ret_WGM_ptr(uint8_t timer_no);
uint8_t ret_CSX_ptr(uint8_t timer_no);

uint16_t serial_rx_ESC_seq();             // AVR 244.
void term_Send_Val_as_Digits(uint8_t val);
void term_Send_16_as_Digits(uint16_t val);	// Not avr 244

void term_Clear_ALL();                    // Clear whole screen
void term_Clear_Top();                    // Clear top of screen
void term_Clear_Bottom();                 // Clear bottom of screen
void term_Clear_2_EOL();                  // Clear till end of line
void term_Clear_2_SOL();                  // Clear till start of line
void term_Clear_Line();                   // Clear entire line

void term_Set_Display_Attribute_Mode(uint8_t mode);
void term_Set_Display_Color(uint8_t FG_O_BG, uint8_t color);
void term_Set_Cursor_Pos(uint8_t row, uint8_t col);
void term_Move_Cursor(uint8_t distance, uint8_t direction);

void termCurPosB(uint8_t row, uint8_t col);

void term_Save_Cursor_Pos();
void term_Recall_Cursor_Pos();

void term_Set_Scroll_All();
void term_Set_Scroll_Mode_Limit(uint8_t start, uint8_t end);      // Set Scroll Limits
void term_Print_Screen();

// END HEAD

void init_gp_timers();
void init_timer_1();

uint8_t main_menu();
void goto_shell();
void goto_help();
void goto_credits();

typedef struct{
	uint8_t OPCODE;
	uint16_t DATA;
} INSTRUCT_STRUCT;


typedef struct{
	uint8_t *addr;
	uint16_t data;
} COMPILED_INSTR;

uint8_t parse_entry(char *user_entry, uint8_t run_instantly, INSTRUCT_STRUCT *INS_OUT);
uint8_t interpret(INSTRUCT_STRUCT *operation);

volatile uint8_t WAIT_FLAG_T2 = 0;


#define Z_LINE_LEN	20
#define Z_LINE_CT	20

void zepto_editor(COMPILED_INSTR* work_space, uint8_t len);
uint8_t last_char(const char str[Z_LINE_LEN][Z_LINE_CT], uint8_t row_);
void zepto_frame_print();
void zepto_frame_cleanup();
void zepto_help_menu();

ISR(TIMER2_COMPA_vect){
	WAIT_FLAG_T2 = 0;
	T2_STOP_TIMER
	T2_NE_COMPA_ISR
}

int main(void){
    init_serial(0);			// 115.2k BAUD 8N1
	
	sei();
	
	// if 1 boot directly to shell
	uint8_t OA_STATE = 1;	// State of currently displayed screen
	
	init_gp_timers();		// Initiate General Delay Timers
	init_timer_1();			// Initiate PWM generation Timer, keep output low
	
	DDRD |= (1 << PIND7);	// Scope trigger indicator strobe, toggle state on output set
	
    while (1){
		switch(OA_STATE){
			case 0:			// Main Menu
				OA_STATE = main_menu();	
			break;
			
			case 1:			// Shell
				fastBorder(1);
				goto_shell();
			break;
	
			case 200:
				goto_help();
			break;
		
			case 255:
				goto_credits();
			break;
		
			default:
				OA_STATE = 0;
			break;	
		}
	}
}

void init_gp_timers(){
	
}

void init_timer_1(){
	// Set Mode: 15, TOP OCR1A, TOV @ TOP, Update OCR1X @ BOTTOM, BOTTOM = 0x0000
	TCCR1B = ((1 << WGM13) | (1 << WGM12));
	TCCR1A = ((1 << COM1B1) | (1 <<WGM11) | (1 << WGM10));
}

// Case 0
uint8_t main_menu(){
	char menu_message[] = "Welcome to ESC and Servo Driver v2!\0";
	char shell_loading_msg[] = "Loading into Shell\0";
	char menu_loading_msg[] = "Press Any Key to Stop\0";
	fastBorder(1);
	
	term_Set_Cursor_Pos(TERM_H / 2 - 1, TERM_W / 2 - sizeof(menu_message) / 2);
	serialWriteStr(menu_message);
	term_Set_Cursor_Pos(TERM_H / 2 + 1, TERM_W / 2 - sizeof(shell_loading_msg) / 2);
	serialWriteStr(shell_loading_msg);
	term_Set_Cursor_Pos(TERM_H / 2 + 3, TERM_W / 2 - sizeof(menu_loading_msg) / 2);
	serialWriteStr(menu_loading_msg);

#define BAR_W			71
#define WAIT_SECONDS	5

	printBarFrame(20, 3, BAR_W);
	updateBarValue(20, 3, BAR_W, 2, 0);
	
	// We have a canned 10ms timer
	// for 5 second loading time:	5 / 0.010 = 500
	// for 1 seconds: 1 / 0.010 = 100
	
	uint8_t to_go__ = 1;						// Where to go after the menu
	
	for(uint8_t s = 0; s < WAIT_SECONDS; s++){
		for(uint8_t n = 0; n < 98; n++){
			T2_10MS_SETUP
			while(WAIT_FLAG_T2);				// Wait for timer to clear
		}
		for(uint8_t m = s; m < ((s + 1) * ((BAR_W - 1) / WAIT_SECONDS)); m++){
			updateBarValue(20, 3, BAR_W, 0, m);
			T2_10MS_SETUP
			while(WAIT_FLAG_T2);
			T2_10MS_SETUP
			while(WAIT_FLAG_T2);
			
		}
	}
#undef BAR_W
#undef WAIT_SECONDS

	//while(1);				// Hold in menu for debug
	return to_go__;
}

// Case 1
#define MAX_ENTRY_LEN		20
#define MAX_LINES			5
char line_entry[MAX_ENTRY_LEN][MAX_LINES] = {0x00};
uint8_t bottom_line = MAX_LINES - 1;

// Shell Modes:
//					0: EXIT
//					1: Print terminal history
//					2: Accept Inputs (resting state)
//					3: RES
//					4: Syntax Error on Entry

void goto_shell(){
	char tmp_buf[MAX_ENTRY_LEN + 1] = {0x00};
	uint8_t wr_ptr = 0;
	uint8_t shell_mode = 1;
	uint8_t cur_pos_X = 3;
	uint8_t cur_pos_Y = MAX_LINES + 1;
	uint8_t line_to_print = 0;
	
	const char sh__nm[] = "SHELL\0";
	
	while(shell_mode){
		if(shell_mode == 4) shell_mode = 1;			// MAKE A HANDLER FOR ERROR ON INPUT
		if(shell_mode == 1){									// Indicates a BUFFER PRINT status event
			cur_pos_Y = MAX_LINES + 1;
			line_to_print = bottom_line;
			for(uint8_t n = 0; n < MAX_LINES; n++){				// Print terminal output buffer
				//cur_pos_Y -= n;
				term_Set_Cursor_Pos(cur_pos_Y - n, cur_pos_X);
				
				for(uint8_t c = 0; c < MAX_ENTRY_LEN; c++){
					serialWrite(' ');
				}
				
				term_Set_Cursor_Pos(cur_pos_Y - n, cur_pos_X);
				
				for(uint8_t c = 0; c < MAX_ENTRY_LEN; c++){
					serialWrite(line_entry[c][line_to_print]);
				}
				
				line_to_print -= 1;
				if(line_to_print > MAX_LINES - 1){
					line_to_print = MAX_LINES - 1;
				}
				
				
			}
			shell_mode = 2;	
		}
		
		term_Set_Cursor_Pos(3, TERM_W - sizeof(sh__nm));
		serialWriteStr(sh__nm);
		
		term_Set_Cursor_Pos(MAX_LINES + 3, 3);
		for(uint8_t n = 0; n < MAX_ENTRY_LEN; n++){
			serialWrite(' ');
		}
		term_Set_Cursor_Pos(MAX_LINES + 3, 3);
		serialWriteStr(tmp_buf);
		
		
		term_Set_Cursor_Pos(TERM_H / 2, TERM_W / 2);
		term_Send_Val_as_Digits(bottom_line);
		
		term_Set_Cursor_Pos(TERM_H / 2 + 2, TERM_W / 2);
		serialWrite('X');
		serialWrite(' ');
		term_Send_Val_as_Digits(cur_pos_X);
		term_Set_Cursor_Pos(TERM_H / 2 + 4, TERM_W / 2);
		serialWrite('Y');
		serialWrite(' ');
		term_Send_Val_as_Digits(cur_pos_Y);
		
		term_Set_Cursor_Pos(MAX_LINES + 3, 2);
		serialWrite('>');
		
		term_Set_Cursor_Pos(MAX_LINES + 3, 3 + wr_ptr);
		
		// Current buffer now printed, now go into entry mode
		uint16_t read_val = serial_rx_ESC_seq();
		if(read_val == ENTER_KEY){							// Enter Key Press Event
			if(tmp_buf[0]){
				bottom_line += 1;
				if(bottom_line > MAX_LINES - 1){
					bottom_line = 0;
				}
				
				shell_mode = parse_entry(tmp_buf, 1, NULL);
				
				for(uint8_t n = 0; n < MAX_ENTRY_LEN; n++){
					if(tmp_buf[n]){
						line_entry[n][bottom_line] = tmp_buf[n];	
					} else {
						line_entry[n][bottom_line] = 0x00;		// Necessary to reset screen/history buffer contents
					}
					tmp_buf[n] = 0x00;
				}
				tmp_buf[MAX_ENTRY_LEN] = 0x00;
				wr_ptr = 0;
			}
		} else
		if(!(read_val & 0xFF)){								// Arrow Key Press Event
			switch(read_val >> 8){
				case MOVE_LEFT:
					if(wr_ptr) wr_ptr -= 1;
				break;
				case MOVE_RIGHT:
					if(tmp_buf[wr_ptr] && wr_ptr < MAX_ENTRY_LEN) wr_ptr += 1;
				break;
				case MOVE_UP:
					wr_ptr = 0;
					for(uint8_t n = 0; n < MAX_ENTRY_LEN; n++){
						tmp_buf[n] = line_entry[n][bottom_line];
						if(tmp_buf[n]){
							wr_ptr += 1;
						}
					}
					for(uint8_t n = MAX_ENTRY_LEN - 1; n != 0; n--){
						if(tmp_buf[n] != ' '){
							break;
						}
						wr_ptr -= 1;
					}
					
				break;
			}
		} else {											// Text Entry Event
			switch(read_val){
				case 8:										// Backspace
					if(wr_ptr){
						wr_ptr -= 1;
						for(uint8_t n = wr_ptr; n < MAX_ENTRY_LEN; n++){
							tmp_buf[n] = tmp_buf[n + 1];
						}
						tmp_buf[MAX_ENTRY_LEN] = '\0';	
					}
				break;
				
				default:									// Generic Text Entry
					if(wr_ptr < MAX_ENTRY_LEN){	
						tmp_buf[wr_ptr] = read_val;
						wr_ptr += 1;
					}
				break;
			}
		}
		
		
		cur_pos_X = 3;
		cur_pos_Y = MAX_LINES + 2;
	}
}

uint8_t parse_entry(char *user_entry, uint8_t run_instantly, INSTRUCT_STRUCT *INS_OUT){
	//term_Set_Cursor_Pos(5, 38);
	//serialWrite('S');
	//serialWrite(' ');
	
	
	uint8_t rd_ptr = 0;
	for(uint8_t n = rd_ptr; n < MAX_ENTRY_LEN; n++){
		if(user_entry[n] != 0x00 && user_entry[n] != ' '){
			rd_ptr = n;
			break;
		}
	}
	
	//term_Set_Cursor_Pos(5, 51);
	//serialWrite('R');
	//term_Send_Val_as_Digits(rd_ptr);
	
	char lead_letter = user_entry[rd_ptr];
	char arg_0_tmp[8] = {0x00};
	uint8_t arg_0_rd_ptr = 0;
	uint8_t scalar_ = 0;		// 0 == us, 1 == ms, 2 == s
	
	uint8_t parse_stage = 0;
	for(uint8_t n = 0; n < MAX_ENTRY_LEN; n++){
		if(!user_entry[n]) break;
		switch(parse_stage){
			case 0:		// Skip to space
				if(user_entry[n] == ' '){
					if(lead_letter == 't' || lead_letter == 'z'){
						parse_stage = 2;		// Collect Text input
					} else {
						parse_stage = 1;		// Collect Numeric input
					}
				}
			break;
			case 1:
				if(user_entry[n] == '.' || (user_entry[n] >= '0' && user_entry[n] <= '9')){
					arg_0_tmp[arg_0_rd_ptr] = user_entry[n];
					arg_0_rd_ptr += 1;
				} else 
				if(user_entry[n] == 's'){
					scalar_ = 2;
				} else
				if(user_entry[n] == 'm'){
					scalar_ = 1;
				}
			break;
			case 2:
				if(user_entry[n] >= 'A' && user_entry[n] <= 'Z') user_entry[n] -= ('A' - 'a');
				if(user_entry[n] >= 'a' && user_entry[n] <= 'z'){
					arg_0_tmp[arg_0_rd_ptr] = user_entry[n];
					arg_0_rd_ptr += 1;
				}
			break;
		}
	}
	
	INSTRUCT_STRUCT INSTR;
	
	switch(lead_letter){
		// Output
		case 'O':
		case 'o':
			INSTR.OPCODE = 0;
		break;
		
		// Freq
		case 'F':
		case 'f':
			INSTR.OPCODE = 1;
		break;
		
		// Period
		case 'P':
		case 'p':
			INSTR.OPCODE = 2;
		break;
		
		// Duty
		case 'D':
		case 'd':
			INSTR.OPCODE = 3;
		break;
		
		// HI_TIME
		case 'H':
		case 'h':
			INSTR.OPCODE = 4;
		break;
		
		// STALL
		case 'S':
		case 's':
			INSTR.OPCODE = 5;
		break;
		
		// TYPE
		case 'T':
		case 't':
			INSTR.OPCODE = 6;
		break;
		
		// Zepto
		case 'Z':
		case 'z':
			INSTR.OPCODE = 7;
		break;
		
		default:
			// Syntax error
			rd_ptr = 0xFF;
		break;
	}
	
	if(rd_ptr == 0xFF) return 4;
	

	// Remember:
	//	PB2 == COMPB
	//	COMPA == TOP
	//  f = 16MHz / ( pre * ( 1 + TOP ) )
	//		Pre Values Range at 65535 TOP:
	//			1		244 Hz
	//			8		30.51
	//			64		3.81
	//			256		0.954
	//			1024	0.238
	//term_Set_Cursor_Pos(10, 50);
	//serialWrite('R');
	//serialWrite('D');
	//serialWrite('\t');
	//term_Send_Val_as_Digits(rd_ptr);
	
	term_Set_Cursor_Pos(15, 3);
	serialWrite('A');
	serialWrite(' ');
	for(uint8_t q = 0; q < 8; q++){
		serialWrite(' ');
	}
	term_Set_Cursor_Pos(15, 5);
	for(uint8_t q = 0; q < 8; q++){
		serialWrite(arg_0_tmp[q]);
	}
	
	if(INSTR.OPCODE > 0 && INSTR.OPCODE < 6){
		arg_0_tmp[7] = 0x00;
		float tmp_ = (float)(strtod(arg_0_tmp, NULL));
		//float tmp_ = (float)(atof(arg_0_tmp));
		
		term_Set_Cursor_Pos(17, 3);
		serialWrite('F');
		serialWrite(' ');
		term_Send_16_as_Digits((uint16_t)tmp_);
		
		// Scale time measurements
		
		if(INSTR.OPCODE == 4){
			if(scalar_ == 0x00){
				// us
				tmp_ = tmp_ / 1000000.0f;
			} else
			if(scalar_ == 0x01){
				// ms
				tmp_ = tmp_ / 1000.0f;
			} else {}	
		}
		
		
		switch(INSTR.OPCODE){
			case 1:					// Freq, Fill lower 8 of DATA with TCCR1B contents to set prescale freq
			case 2:					// Period, inverse of frequency
			case 4:					// Hi Time, destination OCR1B
				if(INSTR.OPCODE != 1){	// period -> freq, time hi -> 'freq' equiv
					tmp_ = 1.0f / tmp_;
					if(INSTR.OPCODE != 4) INSTR.OPCODE = 1;	// Convert back to FREQ from PERIOD
				}
				
				if(tmp_ > 244.0f){		// Div 1
					// No SHIFT bit
					scalar_ = 1;
					tmp_ += 0.3f;
				} else
				if(tmp_ > 30.6f){		// Div 8
					// Add SHIFT bit
					INSTR.OPCODE |= (1 << 7);
					scalar_ = 8;
					tmp_ += 0.03f;
				} else {}
				
				
				
				tmp_ /= 16000000.0f;
				tmp_ = 1.0f / tmp_;
				tmp_ /= (float)scalar_;
				INSTR.DATA = ((uint16_t)tmp_) - 1;		// TOP value, OCR1A
				
			break;		
			
			case 3:					// Duty
				INSTR.DATA = (uint16_t)(65535.0f * (tmp_ / 100.0f));
			break;
			
			case 5:					// Stall (Delay)
				//INSTR.DATA = (uint16_t) (tmp_ * 1000.0f);		// Convert to ms of delay
				INSTR.DATA = (uint16_t) (tmp_);					// ms of delay
			break;
		}
	} else
	if(INSTR.OPCODE == 7){
		// Open Zepto
		INSTR.DATA = 1;
	} else
	if(INSTR.OPCODE == 0x00){
		if(arg_0_tmp[0] == '1' || arg_0_tmp[1] == 'N' || arg_0_tmp[1] == 'n'){
			INSTR.DATA = 1;		// Output on
		} else {
			INSTR.DATA = 0;		// Output Off
		}
	} else
	if(INSTR.OPCODE == 6){
		scalar_ = 0;
		for(uint8_t q = 0; q < MAX_ENTRY_LEN; q++){
			if(arg_0_tmp[q] > 0 && arg_0_tmp[q] != ' '){
				scalar_ = q;
				break;
			}
		}
		
		//term_Set_Cursor_Pos(3, 55);
		//serialWrite('S');
		//serialWrite(' ');
		//term_Send_Val_as_Digits(scalar_);
		
		if(arg_0_tmp[scalar_] == 'e' || arg_0_tmp[scalar_] == 'E'){		// ESC Default: 1500us Center, 400Hz
			INSTR.DATA = 0x01;
		} else
		if(arg_0_tmp[scalar_] == 's' || arg_0_tmp[scalar_] == 'S'){		// Servo Default: 1500us Center, 50Hz
			INSTR.DATA = 0x02;
		} else {
			// Error state
			INSTR.DATA = 0x00;
		}
	} else {}
	
	term_Set_Cursor_Pos(18, 3);
	serialWrite('I');
	serialWrite(' ');
	term_Send_Val_as_Digits(INSTR.OPCODE);
	serialWrite(' ');
	term_Send_16_as_Digits(INSTR.DATA);
	
	if(run_instantly){
		return interpret(&INSTR);
	} else {
		INS_OUT->OPCODE = INSTR.OPCODE;
		INS_OUT->DATA = INSTR.DATA;
		return 1;
	}
}


uint8_t interpret(INSTRUCT_STRUCT *operation){
	uint16_t ret_val = 1;
	term_Set_Cursor_Pos(20, 3);
	serialWrite('O');
	serialWrite('P');
	serialWrite(' ');
	term_Send_Val_as_Digits(operation->OPCODE);
	
	switch(operation->OPCODE){
		case 0:					// Output Set on PB2
			if(operation->DATA)	DDRB |= (1 << PINB2);
			else DDRB &= ~(1 << PINB2);
			TOGGLE_INDIC_STROBE
		break;
		
		case 1:					// Set Frequency, value passed as data == COMPA (x1 pre)
		case 129:				// SHIFT value of set freq (x8 pre)
			if(operation->OPCODE == 129){
				// Pre of 8
				//TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));	// Clear prescalar
				CLEAR_T1_PRE
				//TCCR1B |= (1 << CS11);									// Set prescale 8
				SET_T1_PRE_8
			} else {
				// Pre of 1
				//TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));	// Clear prescalar
				//TCCR1B |= (1 << CS10);									// Set prescale 1
				CLEAR_T1_PRE
				SET_T1_PRE_1
			}
			OCR1A = operation->DATA;									// Set TOP
			TOGGLE_INDIC_STROBE
		break;
		
		case 3:					// Duty Set
			ret_val = (uint16_t)(((float)operation->DATA / 65535.0f) * (float)OCR1A);
			/*
			term_Set_Cursor_Pos(20, 25);
			serialWrite('V');
			serialWrite(' ');
			term_Send_16_as_Digits(ret_val);
			*/
			OCR1B = ret_val;
			ret_val = 1;
		break;
		
		case 4:	// Hi Time
			OCR1B = operation->DATA;
			TOGGLE_INDIC_STROBE
		break;
		
		case 5:	// Delay
		//	T2_1MS_SETUP
			
			for(uint16_t a = 0; a < operation->DATA; a++){
				T2_1MS_SETUP_ENC
				while(WAIT_FLAG_T2);
			}
			
			/*
			for(uint8_t a = 0; a < ((uint8_t)(operation->DATA >> 8)); a++){
				for(uint8_t b = 0; b < ((uint8_t)(operation->DATA)); b++){
					TIFR2 |= (1 << TOV2);
					T2_BEGIN_1MS
					while(WAIT_FLAG_T2);
				}
			}
			*/
		break;
		case 6:	// Type Set
			if(operation->DATA){		// > 0x00 is valid type, 0x00 is error on set
				switch(operation->DATA){
					case 1:				// ESC, 400Hz, 1500us Center
						CLEAR_T1_PRE
						SET_T1_PRE_1
						// SHould be 39999, tuned value below
						OCR1A = 39978;	// 400 Hz TOP
						// Should be 23999, tuned value below
						OCR1B = 23986;  // 1500us COMP value pulse
					break;
					case 2:				// Servo, 50Hz, 1500us Center
						CLEAR_T1_PRE
						SET_T1_PRE_8
						OCR1A = 39999;	// 50 Hz TOP (x8 pre)
						OCR1B = 2999;	// 1500us COMP value pulse
					break;
				}
			}
		break;
		case 7:	// Zepto
			zepto_editor(NULL, 0);
		break;
		
		default:
			ret_val = 4;		// Syntax error or unrecognized instruction
		break;	
	
	}
	
	
	return (uint8_t)ret_val;
}


// Case 200
void goto_help(){
	// lol no help yet
}

// Case 255
void goto_credits(){
	// Me
}

  //////////////////////////////////////////////////////////////////////////
 //							ZEPTO EDITOR								 //
//////////////////////////////////////////////////////////////////////////

#define ZEPTO_W		35
#define ZEPTO_H		2



char zepto_array[Z_LINE_LEN][Z_LINE_CT] = {0x00};

void zepto_editor(COMPILED_INSTR* work_space, uint8_t len){
	zepto_frame_cleanup();
	zepto_frame_print();
	uint16_t read_val;
	uint8_t sm_rval;
	uint8_t ins_mode = 1;		// If 1 normal cursor, otherwise overwrite anything
	// Modes:
	//	0 - Exit
	//	1 - Input Collection
	//	2 - Save Data
	//	3 - Help Menu
	//	4 - EEPROM Read
	//	5 - EEPROM Save
	//	6 - RES
	//	7 - RES
	//	8 - EXT Read	(external storage 0) (SD, FLASH, EEPROM, etc..)
	//	9 - EXT Save	(external storage 0) (SD, FLASH, EEPROM, etc..)
	//	....
	//	16  - Compile program to output buffer
	//	17	- Interpret program in place
	//	....
	//	100 - Print Text Buffer to Screen
	//  101 - Update Cursor
	//	....
	//	105 - Update Cursor Mode Text (INS vs. OVR)
	uint8_t zepto_mode = 100;
	
	// Line length of 20
	// Max lines 20
	// zepto array
	char zep_line_arr[Z_LINE_LEN] = {0x00};
	
	uint8_t cursor_x = 0;
	uint8_t cursor_y = 0;
	
	while(zepto_mode){
		/*
		term_Set_Cursor_Pos(3,3);
		serialWrite('X');
		serialWrite(' ');
		term_Send_Val_as_Digits(cursor_x);
		term_Set_Cursor_Pos(5,3);
		serialWrite('Y');
		serialWrite(' ');
		term_Send_Val_as_Digits(cursor_y);
		*/
		switch(zepto_mode){
			////////////////////////////////////////////////////////////////////////////
			case 1:		// General Purpose Input
				read_val = serial_rx_ESC_seq();
				term_Set_Cursor_Pos(21, 3);
				term_Send_16_as_Digits(read_val);
				
				if(read_val & 0xFF){				// Normal Entry
					sm_rval = (uint8_t)read_val;
					
					if(sm_rval >= 'A' && sm_rval <= 'Z'){
						sm_rval -= 'A' - 'a';
					}
					if((sm_rval >= 'a' && sm_rval <= 'z')
							|| (sm_rval == ' ')
							|| (sm_rval == '.')
							|| (sm_rval >= '0' && sm_rval <= '9')){	// Standard Text
						if(cursor_x < Z_LINE_LEN - 1){
							if(ins_mode){
								for(uint8_t n = Z_LINE_LEN; n != cursor_x; n--){
									zepto_array[n - 1][cursor_y] = zepto_array[n - 2][cursor_y];
								}
							}
							
							zepto_array[cursor_x][cursor_y] = sm_rval;	
							
							
							cursor_x += 1;	
							zepto_mode = 100;	// Print new array
						}
					} else
					if(sm_rval == ENTER_KEY){				// Enter Key Pressed
						if(cursor_y < Z_LINE_CT - 1){
							cursor_y += 1;
							cursor_x = 0;
							zepto_mode = 100;	// Print new array
						}
					} else
					if(sm_rval == BACKSPACE){
						if(cursor_x > 0){
							for(uint8_t n = cursor_x - 1; n < Z_LINE_LEN - 1; n++){
								zepto_array[n][cursor_y] = zepto_array[n + 1][cursor_y];
							}
							cursor_x -= 1;
							zepto_mode = 100;
						} else {
							zepto_mode = 101;			// Redraw cursor
						}
					} else
					if(sm_rval == DELETE){
						for(uint8_t n = cursor_x; n < Z_LINE_LEN - 1; n++){
							zepto_array[n][cursor_y] = zepto_array[n + 1][cursor_y];
						}
						zepto_mode = 100;
					} else
					if(sm_rval == '~'){
						ins_mode = (ins_mode) ? 0 : 1;
						zepto_mode = 105;
					} else
					if(sm_rval == CTRL_R){
						// Run in place
						zepto_mode = 17;
					} else
					if(sm_rval == CTRL_A){
						// Help Menu
						zepto_mode = 3;
					} else
					if(sm_rval == CTRL_X){
						// Quit
						zepto_mode = 0;
					}
				} else {							// Arrow Key Entry
					sm_rval = (uint8_t)(read_val >> 8);
					switch(sm_rval){
						case MOVE_UP:
							if(cursor_y > 0){
								cursor_y -= 1;
								cursor_x = last_char(zepto_array, cursor_y);
							}
						break;
						case MOVE_DOWN:
							if(cursor_y < Z_LINE_CT - 1){
								cursor_y += 1;
								cursor_x = last_char(zepto_array, cursor_y);
							}
						break;
						case MOVE_LEFT:
							if(cursor_x > 0){
								cursor_x -= 1;
							}
						break;
						case MOVE_RIGHT:
							if(cursor_x < Z_LINE_LEN - 1){
								cursor_x += 1;
							}
						break;
					}
					zepto_mode = 100;	// Print new array
				}
				
			break;
			////////////////////////////////////////////////////////////////////////////
			case 2:		// Save Data to work_space
			
				zepto_mode = 1;
			break;
			////////////////////////////////////////////////////////////////////////////
			case 3:		// Help Menu (lol)
				zepto_help_menu();
				zepto_mode = 101;
			break;
			// Cases >3 not used with default config
			
			////////////////////////////////////////////////////////////////////////////
			case 16:											// Compile
			case 17:											// Interpret Program in Place
				for(uint8_t n = 0; n < Z_LINE_CT; n++){
					if(zepto_array[0][n]){
						for(uint8_t q = 0; q < Z_LINE_LEN; q++){
							if((zepto_array[q][n] >= 'a' && zepto_array[q][n] <= 'z')
								|| (zepto_array[q][n] >= '0' && zepto_array[q][n] <= '9')
								|| (zepto_array[q][n] == '.') || zepto_array[q][n] == ' ')
								{
									zep_line_arr[q] = zepto_array[q][n];		
								} else {
									break;
								}
							
						}
						if(zepto_mode == 17){							// Interpret
							parse_entry(zep_line_arr, 1, NULL);	
						} else {										// Compile
							
						}
						
						for(uint8_t p = 0; p < Z_LINE_LEN; p++){
							zep_line_arr[p] = 0x00;
						}
					}
				}
				
				zepto_mode = 1;
			break;
			////////////////////////////////////////////////////////////////////////////
			case 100:	// Print zepto_array to screen
				for(uint8_t n = 0; n < Z_LINE_CT; n++){
					term_Set_Cursor_Pos(n + ZEPTO_H + 1, ZEPTO_W + 5);
					for(uint8_t m = 0; m < Z_LINE_LEN; m++){
						if(zepto_array[m][n] > 0){
							serialWrite(zepto_array[m][n]);	
						} else {
							serialWrite(' ');					// Fill 0x00 with spaces for visualization
						}
						
					}
				}
				
			//break;
			
			case 101:
				term_Set_Cursor_Pos(cursor_y + ZEPTO_H + 1, cursor_x + ZEPTO_W + 5);
				zepto_mode = 1;
			break;
			
			case 105:	// Update cursor mode text
				term_Set_Cursor_Pos(TERM_H - ZEPTO_H, TERM_W - 5);
				if(ins_mode){
					serialWrite('I');
					serialWrite('N');
					serialWrite('S');
					} else {
					serialWrite('O');
					serialWrite('V');
					serialWrite('R');
				}
				
				term_Set_Cursor_Pos(cursor_y + ZEPTO_H + 1, cursor_x + ZEPTO_W + 5);
				zepto_mode = 1;
			break;
			////////////////////////////////////////////////////////////////////////////
			default:
				// Error on mode set, exit program
				zepto_mode = 0x00;
			break;	
		}
	}
	
	zepto_frame_cleanup();
}



uint8_t last_char(const char str[Z_LINE_LEN][Z_LINE_CT], uint8_t row_){
	uint8_t n;
	for(n = 0; n < Z_LINE_LEN - 1; n++){
		if(!str[n][row_]) break;
	}
	return n ;
}

void zepto_frame_print(){
	const char zepto_nametag[] = "ZEPTO\0";
	
	for(uint8_t n = 0; n < Z_LINE_CT; n++){
		term_Set_Cursor_Pos(n + ZEPTO_H + 1, ZEPTO_W + 1);
		term_Send_Val_as_Digits(n + 1);
	}
	
	for(uint8_t n = ZEPTO_H; n < TERM_H; n++){
		term_Set_Cursor_Pos(n, ZEPTO_W);
		serialWrite('#');
		serialWrite(' ');
	}
	
	term_Set_Cursor_Pos(ZEPTO_H + 1, TERM_W - sizeof(zepto_nametag));
	serialWriteStr(zepto_nametag);
}

void zepto_frame_cleanup(){
	for(uint8_t n = ZEPTO_H; n < TERM_H; n++){
		term_Set_Cursor_Pos(n, ZEPTO_W);
		
		for(uint8_t m = ZEPTO_W; m < TERM_W - 1; m++){
			serialWrite(' ');		
		}
	}	
}

void zepto_help_menu(){
	static uint8_t wr_o_cl = 0;
	
	const char zep_help_0[] = "CTRL+R: Interpret\0";
	const char zep_help_1[] = "CTRL+E: Compile\0";
	const char zep_help_2[] = "CTRL+X: Exit\0";
	
	wr_o_cl = (wr_o_cl) ? 0 : 1;
	
	// Help -> TERM_W - 18
	if(wr_o_cl){
		term_Set_Cursor_Pos(TERM_H - 10, TERM_W - 18);
		serialWriteStr(zep_help_0);
		term_Set_Cursor_Pos(TERM_H - 9, TERM_W - 18);
		serialWriteStr(zep_help_1);
		term_Set_Cursor_Pos(TERM_H - 8, TERM_W - 18);
		serialWriteStr(zep_help_2);	
	} else {
		for(uint8_t n = 0; n < 3; n++){
			term_Set_Cursor_Pos(TERM_H - (10 - n), TERM_W - 18);
			for(uint8_t m = 0; m < 17; m++){
				serialWrite(' ');
			}
		}
	}
	
}

#undef Z_LINE_LEN
#undef Z_LINE_CT

#undef ZEPTO_W
#undef ZEPTO_H




























































































































// LIVRARY
void fastBorder(uint8_t clr_scrn){				// No EX-ASCII support, too lazy for that
	if(clr_scrn){
		term_Clear_ALL();
	}
	
	for(uint8_t n = 1; n < TERM_W + 1; n++){
		term_Set_Cursor_Pos(1, n);
		serialWrite(GFX_CHAR);
		
		term_Set_Cursor_Pos(TERM_H, n);
		serialWrite(GFX_CHAR);
	}
	
	for(uint8_t n = 1; n < TERM_H + 1; n++){
		term_Set_Cursor_Pos(n, 1);
		serialWrite(GFX_CHAR);
		
		term_Set_Cursor_Pos(n, TERM_W);
		serialWrite(GFX_CHAR);
	}
}

void printBarFrame(uint8_t top, uint8_t right, uint8_t internalWidth){
	term_Set_Cursor_Pos(top, right);
	
	serialWrite('+');

	for(uint8_t n = 0; n < internalWidth; n++){
		serialWrite('-');
	}

	serialWrite('+');

	term_Set_Cursor_Pos(top + 1, right);
	serialWrite('|');

	term_Set_Cursor_Pos(top + 1, right + 1 + internalWidth);
	serialWrite('|');

	term_Set_Cursor_Pos(top + 2, right);
	serialWrite('+');

	for(uint8_t n = 0; n < internalWidth; n++){
		serialWrite('-');
	}

	serialWrite('+');
	term_Set_Cursor_Pos(TERM_H, TERM_W);
}

void updateBarValue(uint8_t top, uint8_t right, uint8_t internalWidth, uint8_t oldPos, uint8_t newPos){
	if(newPos > internalWidth){
		return;
	}
	term_Set_Cursor_Pos(top + 1, right + 1 + newPos);
	serialWrite('#');

	term_Set_Cursor_Pos(top + 1, right + 1 + oldPos);
	serialWrite(' ');

	term_Set_Cursor_Pos(TERM_H, TERM_W);
}

uint8_t ret_WGM_ptr(uint8_t timer_no){			// Return the WGM mode of Timer 1/2
	uint8_t tmp__;
	if(timer_no == 1){
		tmp__ = TCCR1A & ((1 << WGM11) | (1 << WGM10));
		tmp__ |= (TCCR1B & ((1 << WGM13) | (1 << WGM12))) >> 1;
		// Should now have a 4 bit value consisting of every WGM bit
		switch(tmp__){
			case 0:					// Disconnected
			return 0;
			case 11:				// Phase Correct, TOP at OCR1A
			return 3;
			case 4:					// CTC, TOP at OCR1A
			return 1;
			case 15:				// 16 Bit Fast PWM, TOP at OCR1A
			return 2;
			default:				// IF something weird happens or bad input, turn timer off
			TCCR1A &= ~((1 << WGM11) | (1 << WGM10));
			TCCR1B &= ~((1 << WGM13) | (1 << WGM12));
			return 0;
		}
	} else
	if(timer_no == 2){
		tmp__ = TCCR2A & ((1 << WGM21) | (1 << WGM20));
		tmp__ |= (TCCR2B & (1 << WGM22)) >> 1;
		// Should now have 3 bit value of every WGM mode timer 2 supports
		// Timer 2 Relevant Modes
		switch(tmp__){
			case 0:
			return 0;		// Disconnected
			case 5:
			return 3;		// 8 Bit Phase Correct, TOP at OCR2A
			case 2:
			return 1;		// CTC, TOP at OCR2A
			case 7:
			return 2;		// 8 Bit Fast PWM, TOP at OCR2A
			default:
			TCCR2A &= ~((1 << WGM21) | (1 << WGM20));
			TCCR2B &= ~(1 << WGM22);
			return 0;
		}
		
		} else {
		return 0;
	}
}

uint8_t ret_CSX_ptr(uint8_t timer_no){			// Return value relating to the current prescalar on Timer 1/2
	if(timer_no == 1){
		switch(TCCR1B & 0b00000111){			// Only some prescale are available for timer 1
			case 0:
			case 1:
			case 2:
			return TCCR1B & 0b00000111;
			case 3:
			return 4;
			case 4:
			return 6;
			case 5:
			return 7;
			default:
			return 0;
		}
	} else
	if(timer_no == 2){
		return TCCR2B & 0b00000111;				// Every prescale is available on timer 2
	}
	return 0;
}





void find_T_(uint8_t timer_num, char *arrayZ, uint8_t arrayZ_size){
	uint8_t ckspd = ret_CSX_ptr(timer_num);
	uint8_t mode_ = ret_WGM_ptr(timer_num);
	
	if(ckspd == 0 || mode_ == 0){
		for(uint8_t n = 0; n < arrayZ_size - 3; n++){
			arrayZ[n] = '0';
		}
		arrayZ[arrayZ_size - 1] = 's';
	} else
	if(timer_num == 1){
		
	} else
	if(timer_num == 2){
		
	}
}

float find_freq_tim_X_(uint8_t timer_no, uint8_t clk_spd__, uint8_t _mode__){
	float cks__;
	switch(clk_spd__){
		case 1:
		cks__ = 16000000.0f;
		break;
		case 2:
		cks__ = 2000000.0f;
		break;
		case 3:
		cks__ = 500000.0f;
		break;
		case 4:
		cks__ = 250000.0f;
		break;
		case 5:
		cks__ = 125500.0f;
		break;
		case 6:
		cks__ = 62500.0f;
		break;
		case 7:
		cks__ = 15625.0f;
		break;
		default:
		return 0;
		break;
	}
	// CTC Timer 1:		f_OCnA = 16M / (2 * pre * (1 + OCRnA))		SAME for timer 2
	// FPWM Timer 1:	f_OCnX = 16M / (pre * (1 + TOP))			SAME for timer 2
	// PPWM Timer 1:	f_OCnX = 16M / (2 * pre * TOP)				SAME for timer 2
	//
	// TOP == OCR1A and OCR2A
	// Since we divided 16M by pre already we can just do:
	//
	//	CTC:		f = cks__ * (1 / 2(1 + OCRnA))
	//	FPWM:		f = cks__ * (1 / (1 + TOP))
	//	PPWM:		f = cks__ * (1 / (2 * TOP))
	if(timer_no == 1){
		switch(_mode__){
			case 1:											// CTC
			return cks__ * (1 / (2.0f * ((float)OCR1A) + 1.0f));
			case 2:											// Fast PWM
			return cks__ * (1 / ((float)OCR1A));
			case 3:											// Phase Correct
			return cks__ * (1 / (2.0f * ((float)OCR1A + 1.0f)));
			default:
			return 0;
		}
	} else
	if(timer_no == 2){
		switch(_mode__){
			case 1:											// CTC
			return cks__ * (1 / (2.0f * (float)(1 + OCR2A)));
			case 2:											// Fast PWM
			return cks__ * (1 / (float)(OCR2A));
			case 3:											// Phase Correct
			return cks__ * (1 / (2.0f * (float)OCR2A));
			default:
			return 0;
		}
		} else {
		return 0;
	}
}

// dtostrf(float_value, min_width, num_digits_after_decimal, where_to_store_string)

void prep_F_print(uint8_t timer_no, uint8_t clk_spd__, uint8_t _mode__, char *output_f_, char *output_t_, char *output_t_hi_){
	float vv__ = find_freq_tim_X_(timer_no, clk_spd__, _mode__);
	
	float tmp__;
	
	dtostrf(vv__, 12, 3, output_f_);
	dtostrf((1.0f / vv__), 12, 11, output_t_);
	if(timer_no == 1){
		tmp__ = (float)OCR1B / (float)OCR1A;		// Get ratio of ON to OFF time
		dtostrf(tmp__ * (1.0f / vv__), 12, 11, output_t_hi_);
	} else
	if(timer_no == 2){
		tmp__ = (float)OCR2B / (float)OCR2A;		// Get ratio of ON to OFF time
		dtostrf(tmp__ * (1.0f / vv__), 12, 11, output_t_hi_);
	}
}

//////////////////////////////////////////////////////////////////////////
//							LIBRARY FUNCTIONS							 //
//////////////////////////////////////////////////////////////////////////

void init_serial(unsigned int someVal){         // Debug for use with development boards

	// Set Baud rate
	//UBRR0H = (unsigned char)(someVal >> 8);
	//UBRR0L = (unsigned char)(someVal);

	UBRR0L = 3;		// 250k BAUD
	//UBRR0L = 8;		// 115200 BAUD


	// Enable Tx and Rx
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

	// Setup 8N2 format // Change to 8N1 later
	//UCSR0C = (1 << USBS0) | (3 << UCSZ00);

	// 8N1 Format
	UCSR0C = (3 << UCSZ00);
}


void serialWrite(unsigned char data){
	cli();
	while(!(UCSR0A & (1 << UDRE0)));			// Wait for empty transmit buffer
	UDR0 = data;
	sei();
}

uint8_t serialGet(){
	cli();
	while(!((UCSR0A) & (1 << RXC0)));
	return UDR0;
	sei();
}

void serialWriteStr(const char *inpu){
	uint8_t n = 0;
	while(inpu[n]){
		serialWrite(inpu[n]);
		n = n + 1;
	}
}


void serialWriteNl(unsigned char data){
	serialWrite(data);
	serialWrite(NEWLINE);
}

void printStr(uint8_t data){
	if(!data){
		serialWrite('0');
		return;
	}
	uint8_t zero_rejection = 0, tctr__ = 0;

	for(uint16_t n = 1000; n > 0; n/= 10){
		switch(data / n){
			case 0:
			if(zero_rejection){
				serialWrite('0');
			}
			break;
			case 1:
			serialWrite('1');
			zero_rejection = 0x01;
			break;
			case 2:
			serialWrite('2');
			zero_rejection = 0x01;
			break;
			case 3:
			serialWrite('3');
			zero_rejection = 0x01;
			break;
			case 4:
			serialWrite('4');
			zero_rejection = 0x01;
			break;
			case 5:
			serialWrite('5');
			zero_rejection = 0x01;
			break;
			case 6:
			serialWrite('6');
			zero_rejection = 0x01;
			break;
			case 7:
			serialWrite('7');
			zero_rejection = 0x01;
			break;
			case 8:
			serialWrite('8');
			zero_rejection = 0x01;
			break;
			case 9:
			serialWrite('9');
			zero_rejection = 0x01;
			break;
			default:
			break;
		}
		tctr__ += 1;
		
		data = data % n;
	}
}

void printBin8(uint8_t data){
	for(uint8_t n = 0; n < 8; n++){
		uint8_t tm_ = (data >> (7 - n)) & (0x01);
		if(tm_){
			serialWrite('1');
			} else {
			serialWrite('0');
		}
	}
}




uint16_t serial_rx_ESC_seq(){           // If upper 8 bits are full esc code was called
	uint16_t  ret;
	uint8_t   val;

	val = serialGet();

	if(val != 27){                        // Check if ESC
		ret = val;                          // Check for '['
		} else {
		val = serialGet();
		
		if(val != '['){
			ret = val;                       // Return char if not full ESC code
			} else {
			val = serialGet();

			if(val > 0x40 && val < 0x45){   // A - D, arrow keys
				ret = val << 8;               // Store ESC Code char in upper byte
				} else {
				ret = val;                    // Return char if unknown ESC code
			}
		}
	}
	return ret;
}

void term_Send_Val_as_Digits(uint8_t val){
	uint8_t digit = '0';

	while(val >= 100){
		digit += 1;
		val -= 100;
	}
	serialWrite(digit);

	digit = '0';
	while(val >= 10){
		digit += 1;
		val -= 10;
	}
	serialWrite(digit);

	serialWrite('0' + val);
}

void term_Send_16_as_Digits(uint16_t val){
	uint8_t digit = '0';
	
	while(val >= 10000){
		digit += 1;
		val -= 10000;
	}
	serialWrite(digit);
	
	digit = '0';
	
	while(val >= 1000){
		digit += 1;
		val -= 1000;
	}
	serialWrite(digit);
	
	digit = '0';
	
	while(val >= 100){
		digit += 1;
		val -= 100;
	}
	serialWrite(digit);
	
	digit = '0';
	
	while(val >= 10){
		digit += 1;
		val -= 10;
	}
	serialWrite(digit);
	
	serialWrite('0' + val);
}

void term_Clear_ALL(){                    // Clear Screen
	SENDESC
	serialWrite('2');
	serialWrite('J');
}
void term_Clear_Top(){                    // Clear top of screen
	SENDESC
	serialWrite('1');
	serialWrite('J');
}
void term_Clear_Bottom(){                 // Clear bottom of screen
	SENDESC
	serialWrite('J');
}
void term_Clear_2_EOL(){                  // Clear till end of line
	SENDESC
	serialWrite('K');
}
void term_Clear_2_SOL(){                  // Clear till start of line
	SENDESC
	serialWrite('1');
	serialWrite('K');
}
void term_Clear_Line(){                   // Clear entire line
	SENDESC
	serialWrite('2');
	serialWrite('K');
}

void term_Set_Display_Attribute_Mode(uint8_t mode){
	SENDESC
	serialWrite(mode);
	serialWrite('m');
}
void term_Set_Display_Color(uint8_t FG_O_BG, uint8_t color){
	SENDESC
	serialWrite(FG_O_BG);
	serialWrite(color);
	serialWrite('m');
}
void term_Set_Cursor_Pos(uint8_t row, uint8_t col){
	SENDESC
	term_Send_Val_as_Digits(row);
	serialWrite(';');
	term_Send_Val_as_Digits(col);
	serialWrite('H');
}
void term_Move_Cursor(uint8_t distance, uint8_t direction){
	SENDESC
	term_Send_Val_as_Digits(distance);
	serialWrite(direction);
}
void term_Save_Cursor_Pos(){
	SENDESC
	serialWrite('s');
}
void term_Recall_Cursor_Pos(){
	SENDESC
	serialWrite('u');
}


void termCurPosB(uint8_t row, uint8_t col){
	term_Set_Cursor_Pos(1, 1);
	uint8_t n;
	
	for(n = 1; n < col; n++){
		term_Move_Cursor(1, MOVE_RIGHT);
	}
	
	for(n = 1; n < row; n++){
		term_Move_Cursor(1, MOVE_DOWN);
		if(!col){
			term_Move_Cursor(1, MOVE_LEFT);
		}
	}
}


void term_Cursor_Visibility(uint8_t en){
	SENDESC
	if(en){
		//serialWrite('?');
		term_Send_Val_as_Digits(25);
		serialWrite(' ');
		serialWrite('h');
		} else {
		//serialWrite('?');
		term_Send_Val_as_Digits(25);
		serialWrite(' ');
		serialWrite('l');
	}
}

void term_Set_Scroll_All(){
	SENDESC
	serialWrite('r');
}
void term_Set_Scroll_Mode_Limit(uint8_t start, uint8_t end){    // Set Scroll Limits
	SENDESC
	term_Send_Val_as_Digits(start);
	serialWrite(';');
	term_Send_Val_as_Digits(end);
	serialWrite('r');
}
void term_Print_Screen(){
	SENDESC
	serialWrite('1');
}
