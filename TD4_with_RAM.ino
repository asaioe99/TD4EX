#include <LiquidCrystal_I2C.h>

#define ROM_IN_0 2
#define ROM_IN_1 3
#define ROM_IN_2 4
#define ROM_IN_3 5
#define ROM_OUT_0 13
#define ROM_OUT_1 12
#define ROM_OUT_2 11
#define ROM_OUT_3 10
#define ROM_OUT_4 9
#define ROM_OUT_5 8
#define ROM_OUT_6 7
#define ROM_OUT_7 6

//命令セット
#define ADD_A_Im 0b0000
#define MOV_A_B  0b0001
#define IN_A_Im  0b0010
#define MOV_A_Im 0b0011
#define MOV_B_A  0b0100
#define ADD_B_Im 0b0101
#define IN_B_Im  0b0110
#define MOV_B_Im 0b0111
#define PUSH_A   0b1000
#define OUT_Im_A 0b1001
#define POP_A    0b1010
#define OUT_A_Im 0b1011
#define ADDS     0b1100
#define JC_Im    0b1101
#define JNC_Im   0b1110
#define JMP_Im   0b1111

//ROMRAMレジスタ等
byte rom[16];
byte ram[16];
byte c_flag = 0;
byte reg_a  = 0b0000;
byte reg_b  = 0b0000;
byte reg_sp = 0b1111;
byte reg_pc = 0b0000;
//実行ステップ数（スコア）
unsigned long score = 0;
//命令フラグ
int opcode;

LiquidCrystal_I2C lcd(0x27, 20, 04);

void rom_select(int a) {
  //アドレスaを入力する。
  digitalWrite(ROM_IN_0, (a & 0b1000) >> 3);
  digitalWrite(ROM_IN_1, (a & 0b0100) >> 2);
  digitalWrite(ROM_IN_2, (a & 0b0010) >> 1);
  digitalWrite(ROM_IN_3, (a & 0b0001) >> 0);
}
//ROMの読み出し。グローバル変数に格納
void rom_read() {
  for (int i = 0b0000; i <= 0b1111; i++) {
    rom_select(i);
    rom[i] = 0;
    rom[i] += digitalRead(ROM_OUT_0) << 0;
    rom[i] += digitalRead(ROM_OUT_1) << 1;
    rom[i] += digitalRead(ROM_OUT_2) << 2;
    rom[i] += digitalRead(ROM_OUT_3) << 3;
    rom[i] += digitalRead(ROM_OUT_4) << 4;
    rom[i] += digitalRead(ROM_OUT_5) << 5;
    rom[i] += digitalRead(ROM_OUT_6) << 6;
    rom[i] += digitalRead(ROM_OUT_7) << 7;
  }
}

void ram_init() {
  for (int i = 0; i < 16; i++) {
    ram[i] = 0b0000;
  }
}

void add_a_im() {
  int tmp = rom[reg_pc] & 0b00001111;
  reg_a += tmp;
  if (reg_a > 0b1111) {
    c_flag = 1;
  } else {
    c_flag = 0;
  }
  reg_a = reg_a % 16;
  reg_pc = (++reg_pc) % 16;
}
void mov_a_b() {
  int tmp = rom[reg_pc] & 0b00001111;
  reg_a = reg_b + tmp;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void in_a_im() {
  reg_a = ram[rom[reg_pc] & 0b00001111];
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void mov_a_im() {
  reg_a = rom[reg_pc] & 0b00001111;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void mov_b_a() {
  reg_b = reg_a;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void add_b_im() {
  int tmp = rom[reg_pc] & 0b00001111;
  reg_b += tmp;
  if (reg_b > 0b1111) {
    c_flag = 1;
  } else {
    c_flag = 0;
  }
  reg_b = reg_b % 16;
  reg_pc = (++reg_pc) % 16;
}
void in_b_im() {
  reg_b = ram[rom[reg_pc] & 0b00001111];
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void mov_b_im() {
  reg_b = rom[reg_pc] & 0b00001111;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void push_a() {
  ram[reg_sp] = reg_a;
  if (reg_sp > 0) {
    reg_sp--;
  } else {
    reg_sp = 0b1111;
  }
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void out_im_a() {
  ram[reg_a] = rom[reg_pc] & 0b00001111;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void pop_a() {
  reg_sp = (++reg_sp) % 16;
  reg_a = ram[reg_sp];
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void out_a_im() {
  ram[rom[reg_pc] & 0b00001111] = reg_a;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void adds() {
  ram[(reg_sp + 2) % 16] += ram[(reg_sp + 1) % 16];
  reg_sp = (++reg_sp) % 16;
  c_flag = 0;
  reg_pc = (++reg_pc) % 16;
}
void jc_im() {
  if (c_flag == 1) {
    reg_pc = rom[reg_pc] & 0b00001111;
  } else {
    reg_pc = (++reg_pc) % 16;
  }
  c_flag = 0;
}
void jnc_im() {
  if (c_flag == 0) {
    reg_pc = rom[reg_pc] & 0b00001111;
  } else {
    reg_pc = (++reg_pc) % 16;
  }
  c_flag = 0;
}
void jmp_im() {
  reg_pc = rom[reg_pc] & 0b00001111;
  c_flag = 0;
}

void lcd_display() {
  char RAM0_5[19];
  char RAM6_B[19];
  char RAMC_Fabsc[21];
  char absc[9];
  char pOPscr[20];
  char f_ram_cng = 0;

  lcd.setCursor(1, 3);
  switch (opcode) {
    case 0:
      snprintf(pOPscr, 20, "%X:ADD  A ,  %X %05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case 1:
      snprintf(pOPscr, 20, "%X:MOV  A ,  B %05d", reg_pc, score);
      break;
    case 2:
      snprintf(pOPscr, 20, "%X:IN   A ,[0%X]%05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      f_ram_cng = 1;
      break;
    case 3:
      snprintf(pOPscr, 20, "%X:MOV  A ,  %X %05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case 4:
      snprintf(pOPscr, 20, "%X:MOV  B ,  A %05d", reg_pc, score);
      break;
    case 5:
      snprintf(pOPscr, 20, "%X:ADD  B ,  %X %05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case 6:
      snprintf(pOPscr, 20, "%X:IN   B ,[0%X]%05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      f_ram_cng = 1;
      break;
    case 7:
      snprintf(pOPscr, 20, "%X:MOV  B ,  %X %05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case 8:
      snprintf(pOPscr, 20, "%X:PUSH A      %05d", reg_pc, score);
      f_ram_cng = 1;
      break;
    case 9:
      snprintf(pOPscr, 20, "%X:OUT  %X ,[ A]%05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      f_ram_cng = 1;
      break;
    case 10:
      snprintf(pOPscr, 20, "%X:POP  A      %05d", reg_pc, score);
      f_ram_cng = 1;
      break;
    case 11:
      snprintf(pOPscr, 20, "%X:OUT  A ,[0%X]%05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      f_ram_cng = 1;
      break;
    case 12:
      snprintf(pOPscr, 20, "%X:ADDS        %05d", reg_pc, score);
      f_ram_cng = 1;
      break;
    case 13:
      snprintf(pOPscr, 20, "%X:JC   %X      %05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case 14:
      snprintf(pOPscr, 20, "%X:JNC  %X      %05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    case 15:
      snprintf(pOPscr, 20, "%X:JMP  %X      %05d", reg_pc, rom[reg_pc] & 0b00001111, score);
      break;
    default:
      break;
  }
  lcd.print(pOPscr);

  if (f_ram_cng == 1) {
    snprintf(RAM0_5, 17, "%X1:%X2:%X3:%X4:%X5:%X", ram[0x0], ram[0x1], ram[0x2], ram[0x3], ram[0x4], ram[0x5]);
    snprintf(RAM6_B, 17, "%X7:%X8:%X9:%XA:%XB:%X", ram[0x6], ram[0x7], ram[0x8], ram[0x9], ram[0xA], ram[0xB]);
    lcd.setCursor(2, 0);
    lcd.print(RAM0_5);
    lcd.setCursor(2, 1);
    lcd.print(RAM6_B);
  }

  snprintf(RAMC_Fabsc, 19, "%XD:%XE:%XF:%Xa%Xb%Xs%Xc%X", ram[0xC], ram[0xD], ram[0xE], ram[0xF], reg_a, reg_b, reg_sp, c_flag);
  lcd.setCursor(2, 2);
  lcd.print(RAMC_Fabsc);
}

void serial_debug() {
  for (int i = 0; i < 16; i++) {
    Serial.print(i, HEX);
    Serial.print(":");
    Serial.print((rom[i] & 0b10000000) >> 7, BIN);
    Serial.print((rom[i] & 0b01000000) >> 6, BIN);
    Serial.print((rom[i] & 0b00100000) >> 5, BIN);
    Serial.print((rom[i] & 0b00010000) >> 4, BIN);
    Serial.print((rom[i] & 0b00001000) >> 3, BIN);
    Serial.print((rom[i] & 0b00000100) >> 2, BIN);
    Serial.print((rom[i] & 0b00000010) >> 1, BIN);
    Serial.println(rom[i] & 0b00000001, BIN);
  }
}

void setup() {
  //debug
  //Serial.begin( 9600 );     // シリアル通信を初期化する。通信速度は9600bps

  // ROM入力pin割り当て
  pinMode(ROM_IN_0, OUTPUT);
  pinMode(ROM_IN_1, OUTPUT);
  pinMode(ROM_IN_2, OUTPUT);
  pinMode(ROM_IN_3, OUTPUT);
  // ROM出力pin割り当て
  pinMode(ROM_OUT_0, INPUT);
  pinMode(ROM_OUT_1, INPUT);
  pinMode(ROM_OUT_2, INPUT);
  pinMode(ROM_OUT_3, INPUT);
  pinMode(ROM_OUT_4, INPUT);
  pinMode(ROM_OUT_5, INPUT);
  pinMode(ROM_OUT_6, INPUT);
  pinMode(ROM_OUT_7, INPUT);

  //LCD初期化
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Ready...");
  delay(1000);
  //高速化のために固定表示部を予めLCDに表示
  lcd.setCursor(0, 0);
  lcd.print("0:");
  lcd.setCursor(0, 1);
  lcd.print("6:");
  lcd.setCursor(0, 2);
  lcd.print("C:");
  lcd.setCursor(0, 3);
  lcd.print("p");
  lcd_display();
  //ROM読み込み
  rom_read();
  //RAM初期化
  ram_init();
  //シリアルモニタデバッグ用表示
  //serial_debug();
}

void loop() {
  rom_select(reg_pc);

  switch (rom[reg_pc] >> 4) {
    case ADD_A_Im:
      opcode = 0;
      lcd_display();
      add_a_im();
      break;
    case MOV_A_B:
      opcode = 1;
      lcd_display();
      mov_a_b();
      break;
    case IN_A_Im:
      opcode = 2;
      lcd_display();
      in_a_im();
      break;
    case MOV_A_Im:
      opcode = 3;
      lcd_display();
      mov_a_im();
      break;
    case MOV_B_A:
      opcode = 4;
      lcd_display();
      mov_b_a();
      break;
    case ADD_B_Im:
      opcode = 5;
      lcd_display();
      add_b_im();
      break;
    case IN_B_Im:
      opcode = 6;
      lcd_display();
      in_b_im();
      break;
    case MOV_B_Im:
      opcode = 7;
      lcd_display();
      mov_b_im();
      break;
    case PUSH_A:
      opcode = 8;
      lcd_display();
      push_a();
      break;
    case OUT_Im_A:
      opcode = 9;
      lcd_display();
      out_im_a();
      break;
    case POP_A:
      opcode = 10;
      lcd_display();
      pop_a();
      break;
    case OUT_A_Im:
      opcode = 11;
      lcd_display();
      out_a_im();
      break;
    case ADDS:
      opcode = 12;
      lcd_display();
      adds();
      break;
    case JC_Im:
      opcode = 13;
      lcd_display();
      jc_im();
      break;
    case JNC_Im:
      opcode = 14;
      lcd_display();
      jnc_im();
      break;
    case JMP_Im:
      opcode = 15;
      lcd_display();
      jmp_im();
      break;
    default:
      break;
  }
  score++;
  if (ram[0] > 3) {
    lcd_display();
    lcd.setCursor(19, 0);
    lcd.print("*");
    for (;;) {
      delay(10000);
    }
  }
}
