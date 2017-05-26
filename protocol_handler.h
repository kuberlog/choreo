#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>
#include "motor.h"
#include "constants.h"
#include "command.h"


#define MOTOR_INDEX short_buffer[0]
#define AMOUNT short_buffer[1]
#define DURATION short_buffer[2]

Motor* motors[NUMBER_OF_MOTORS];
Linear_Procedural_Command_Queue lpcqs[NUM_LPCQ];
Single_Motor_Choreo_Queue smcqs[NUM_SMCQ];




/*
  There are several commands that can be issued:

  All values are in binary and represent single or double byte numbers.

  =============================
  |        MOTORS OFF         |
  =============================
  |  0                        |
  =============================


  =============================
  |        MOTORS ON          |
  =============================
  |  1                        |
  =============================


  =============================
  |            LPCQ           |
  =============================
  |  2                        |
  |  <motor>                  |
  |    <amount><duration>     |
  =============================

  =============================
  |           SMCQ            |
  =============================
  |  3                        |
  |    <motor>                |
  |      <num LPCQs>          |
  |      <amount><duration>   |
  |      <amount><duration>   |
  |      <amount><duration>   |
  =============================

  =============================
  |           MMCQ            |
  =============================
  |  4                        |
  |  <code>                   |
  |   <num_motors>            |
  |     <motor>               |
  |      <num LPCQs>          |
  |       <amount><duration>  |
  |       <amount><duration>  |
  |       <amount><duration>  |
  |     <motor>               |
  |      <num LPCQs>          |
  |       <amount><duration>  |
  |       <amount><duration>  |
  |       <amount><duration>  |
  =============================

  =============================
  |REMAINING_COMMANDS_ON_MOTOR|
  =============================
  |  5<motor>                 |
  =============================

  =============================
  |        POSITIONS          |
  =============================
  |  6                        |
  =============================
  |  <motor0 position>        |
  |  <motor1 position>        |
  |  <motor2 position>        |
  =============================
 */

int create_lpcq(short *short_buffer, int start_pos){

  char char_buffer[5];
  int  i;
  Serial.readBytes(char_buffer, 4);


  //Amount
  AMOUNT = (unsigned char)char_buffer[0] << 8 | (unsigned char)char_buffer[1];

  //Duration
  DURATION = (unsigned char)char_buffer[2] << 8 | (unsigned char)char_buffer[3];

  Serial.print("Amount: ");
  Serial.print(AMOUNT);
  Serial.print("\n");

  Serial.print("Duration: ");
  Serial.print(DURATION);
  Serial.print("\n");

  //Find an available LPCQ
  for(i=0;i<NUM_LPCQ;i++){
    if(!lpcqs[i].is_active()){
      break;
    }
    //If it didn't break, catch it on the way out
    //and increment to indicate that none are ok
    if(i == NUM_LPCQ -1){
      i++;
    }
  }

  //Are there any lpcqs available and is the command within bounds
  if(i != NUM_LPCQ
     &&
     start_pos + AMOUNT >=
     motors[MOTOR_INDEX]->get_lower_bound()
     &&
     start_pos + AMOUNT <=
     motors[MOTOR_INDEX]->get_upper_bound())
    {
      //Make the LPCQ
      lpcqs[i] = Linear_Procedural_Command_Queue(
                  start_pos,
                  start_pos + AMOUNT,
                  DURATION);


      //Return its index
      return i;

  //Pick out the error
  }else if(i == NUM_LPCQ){
    Serial.println("No LPCQs available");
    return -1;
  }else{
    Serial.println("Dest position is out of bounds");
    return -2;
  }
}


void handle_serial_commands
()
{
  byte count;
  char char_buffer[1];
  int  i;

  if(Serial.available() >0){
    count = Serial.readBytes(char_buffer, 1);
    i = char_buffer[0];
    switch(i){
    case(0):



      Serial.println("motors_off");
      digitalWrite(MOTOR_SWITCH_PIN, LOW);
      break;
    case(1):
      Serial.println("motors_on");
      digitalWrite(MOTOR_SWITCH_PIN, HIGH);
      break;
    case(2):
      {
        short short_buffer[5];
        Serial.println("LPCQ");
        Serial.readBytes(char_buffer, 1);
        MOTOR_INDEX = (int)char_buffer[0];
        int start_pos = motors[MOTOR_INDEX]->get_pos();
        i = create_lpcq(short_buffer, start_pos);
        //If successfully created an lpcq:
        if(i>=0){
          motors[MOTOR_INDEX]->add_command_queue(&lpcqs[i]);
        }
      }
      break;

    case(3):
      {
        Serial.println("SMCQ");
        short status;
        short smcq_index;
        short num_lpcqs;
        short short_buffer[5];

        Serial.readBytes(char_buffer, 2);
        MOTOR_INDEX = (int)char_buffer[0];
        Serial.print("Motor Index: ");
        Serial.print(MOTOR_INDEX);
        Serial.print("\n");
        num_lpcqs = (int)char_buffer[1];
        Serial.print("Num Lpcqs: ");
        Serial.print(num_lpcqs);
        Serial.print("\n");
        //Find an available SMCQ
        for(i=0;i<NUM_SMCQ;i++){
          if(!smcqs[i].front()){
            break;

          }
          //If it didn't break, catch it on the way out
          //and increment to indicate that none are ok
          if(i == NUM_SMCQ -1){
            i++;
          }
        }

        //If a ununsed SMCQ was found
        if(i != NUM_SMCQ){
          smcq_index = i;
          Serial.print("SMCQ #: ");
          Serial.print(smcq_index);
          Serial.print("\n");
          smcqs[smcq_index]=Single_Motor_Choreo_Queue();

          int pos_buffer;
          pos_buffer = motors[MOTOR_INDEX]->get_pos();
          for(i=0;i<num_lpcqs;i++){
            status = create_lpcq(short_buffer, pos_buffer);
            if(status>=0){
              Serial.println("Created LPCQ");
              pos_buffer = pos_buffer + AMOUNT;

              smcqs[smcq_index].insert(&lpcqs[status]);
            }
          }
          motors[MOTOR_INDEX]->add_command_queue(&smcqs[smcq_index]);
        }
      }
      break;
    case(4):
      Serial.println("MMCQ");
      break;
    case(5):
      Serial.println("Remaining commands");
      break;
    case(6):
      Serial.println("Positions");
      for(i=0; i<NUMBER_OF_MOTORS; i++){
        Serial.write(motors[i]->get_pos());
      }
      break;
    default:
      Serial.println("Unkown Command");
      break;

    }
  }
}


#endif
