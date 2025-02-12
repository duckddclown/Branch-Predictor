//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Ziyuan Lin";
const char *studentID = "A59025230";
const char *email = "zil102@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 17; // Number of bits used for Global History
int bpType;            // Branch Prediction Type
int verbose;

//Tournament bits.
int ghistoryBits_tournament = 16;
int lhistoryBits = 14;
int pcIndexBits = 12;
int choicerBits = 14;

// TAGE bits.
int predBits = 3;
int tagBits = 8;
int uBits = 2;
int tagepcBits = 13;
int tageIndexBits = 12;
int historyBaseBits = 4;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// TODO: Add your own Branch Predictor data structures here
//
// gshare
uint8_t *bht_gshare;
uint64_t ghistory;

// Alpha
uint8_t* bht_tournament_global;
uint8_t* bht_tournament_local;
uint64_t* lht_tournament;
uint8_t* choicer;

// TAGE
struct tagged_predictor
{
  uint8_t pred;
  uint8_t tag;
  uint8_t u;
};

uint8_t* T_0;
tagged_predictor* T_1;
tagged_predictor* T_2;
tagged_predictor* T_3;
tagged_predictor* T_4;
uint32_t history;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

// gshare functions
void init_gshare()
{
  int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  for (i = 0; i < bht_entries; i++)
  {
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}

uint8_t gshare_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch (bht_gshare[index])
  {
  case WN:
    return NOTTAKEN;
  case SN:
    return NOTTAKEN;
  case WT:
    return TAKEN;
  case ST:
    return TAKEN;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    return NOTTAKEN;
  }
}

void train_gshare(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  // Update state of entry in bht based on outcome
  switch (bht_gshare[index])
  {
  case WN:
    bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    break;
  }

  // Update history register
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
  free(bht_gshare);
}

//Tournament functions.

void init_tournament()
{
  int global_bht_entries = 1 << ghistoryBits_tournament;
  int local_bht_entries = 1 << lhistoryBits;
  int choicer_entries = 1 << choicerBits;
  int pc_entries = 1 << pcIndexBits;
  bht_tournament_global = (uint8_t *)malloc(global_bht_entries * sizeof(uint8_t));
  bht_tournament_local = (uint8_t *)malloc(local_bht_entries * sizeof(uint8_t));
  choicer = (uint8_t *)malloc(choicer_entries * sizeof(uint8_t));
  lht_tournament = (uint64_t *)malloc(pc_entries * sizeof(uint64_t));
  ghistory = 0;
  int i = 0;
  for (i = 0; i < global_bht_entries; i++)
  {
    bht_tournament_global[i] = WN;
  }
  for (i = 0; i < choicer_entries; i++)
  {
    choicer[i] = WN;
  }
  for (i = 0; i < local_bht_entries; i++)
  {
    bht_tournament_local[i] = WN;
  }
  for (i = 0; i < pc_entries; i++)
  {
    lht_tournament[i] = 0;
  }
}

uint8_t tournament_predict(uint32_t pc)
{
  uint32_t pc_entries = 1 << pcIndexBits;
  uint32_t local_bht_entries = 1 << lhistoryBits;
  uint32_t choicer_entries = 1 << choicerBits;
  uint32_t pc_lower_bits = pc & (pc_entries - 1);
  uint64_t local_history_pattern = lht_tournament[pc_lower_bits];
  local_history_pattern = local_history_pattern & (local_bht_entries-1);
  uint8_t local_branch_history = bht_tournament_local[local_history_pattern];
  uint32_t global_bht_entries = 1 << ghistoryBits_tournament;
  pc_lower_bits = pc & (global_bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (global_bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  uint8_t global_branch_history = bht_tournament_global[index];
  uint32_t choicer_index = index & (choicer_entries-1);
  // If choicer is WT or ST, then we choose to use global one.
  if (choicer[choicer_index] > 1)
  {
    switch (global_branch_history)
    {
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in Tournament BHT!\n");
      return NOTTAKEN;
    }
  }
  else
  {
    switch (local_branch_history)
    {
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in Tournament BHT!\n");
      return NOTTAKEN;
    }
  }
}

void train_tournament(uint32_t pc, uint8_t outcome)
{
  uint32_t pc_entries = 1 << pcIndexBits;
  uint32_t pc_lower_bits = pc & (pc_entries - 1);
  uint32_t choicer_entries = 1 << choicerBits;
  uint32_t local_bht_entries = 1 << lhistoryBits;
  uint64_t local_history_pattern = lht_tournament[pc_lower_bits];
  local_history_pattern = local_history_pattern & (local_bht_entries-1);
  uint8_t local_branch_history = bht_tournament_local[local_history_pattern];
  uint32_t global_bht_entries = 1 << ghistoryBits_tournament;
  pc_lower_bits = pc & (global_bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (global_bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  uint8_t global_branch_history = bht_tournament_global[index];
  uint32_t choicer_index = index & (choicer_entries-1);
  switch (local_branch_history)
  {
  case WN:
    bht_tournament_local[local_history_pattern] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_tournament_local[local_history_pattern] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_tournament_local[local_history_pattern] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_tournament_local[local_history_pattern] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in Tournament BHT!\n");
    break;
  }
  switch (global_branch_history)
  {
  case WN:
    bht_tournament_global[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_tournament_global[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_tournament_global[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_tournament_global[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in Tournament BHT!\n");
    break;
  }
  pc_lower_bits = pc & (pc_entries - 1);
  lht_tournament[pc_lower_bits] = ((lht_tournament[pc_lower_bits] << 1) | outcome);
  ghistory = ((ghistory << 1) | outcome);
  uint8_t score = (outcome == TAKEN) ? ST : SN;
  bool update_global = true;
  if (global_branch_history != local_branch_history)
  {
    if (score == ST)
    {
      uint8_t global_bias = score - global_branch_history;
      uint8_t local_bias = score - local_branch_history;
      if (global_bias < local_bias)
      {
        update_global = true;
      }
      else
      {
        update_global = false;
      }
    }
    else
    {
      uint8_t global_bias = global_branch_history - score;
      uint8_t local_bias = local_branch_history - score;
      if (global_bias < local_bias)
      {
        update_global = true;
      }
      else
      {
        update_global = false;
      }
    }
    if (update_global)
    {
      switch (choicer[choicer_index])
      {
      case SN:
        choicer[choicer_index] = WN;
        break;
      case WN:
        choicer[choicer_index] = WT;
        break;
      case WT:
        choicer[choicer_index] = ST;
        break;
      case ST:
        choicer[choicer_index] = ST;
        break;
      default:
        printf("Warning: Undefined state of entry in choicer!\n");
        break;
      }
    }
    else
    {
      switch (choicer[choicer_index])
      {
      case SN:
        choicer[choicer_index] = SN;
        break;
      case WN:
        choicer[choicer_index] = SN;
        break;
      case WT:
        choicer[choicer_index] = WN;
        break;
      case ST:
        choicer[choicer_index] = WT;
        break;
      default:
        printf("Warning: Undefined state of entry in choicer!\n");
        break;
      }
    }
  }
}

void cleanup_tournament()
{
  free(bht_tournament_global);
  free(bht_tournament_local);
  free(lht_tournament);
  free(choicer);
}

// TAGE functions.

uint32_t index_hash(uint32_t pc, uint32_t history, int number)
{
  if (number == 1)
  {
    return ((pc * 31) ^ ((history & 0xF) * 17)) & 0xFF;
  }
  if (number == 2)
  {
    return ((pc * 31) ^ ((history & 0xFF) * 17)) & 0xFF;
  }
  if (number == 3)
  {
    return ((pc * 31) ^ ((history & 0xFFFF) * 17)) & 0xFF;
  }
  if (number == 4)
  {
    return ((pc * 31) ^ (history * 17)) & 0xFF;
  }
  return 0;
}

uint32_t tag_hash(uint32_t pc, uint32_t history, int number)
{
  switch (number)
  {
  case 1:
    return ((pc * 13) ^ ((history & 0xF) * 53)) & 0xFF;
    break;
  case 2:
    return ((pc * 13) ^ ((history & 0xFF) * 53)) & 0xFF;
    break;
  case 3:
    return ((pc * 13) ^ ((history & 0xFFFF) * 53)) & 0xFF;
    break;
  case 4:
    return ((pc * 13) ^ (history * 53)) & 0xFF;
    break;
  default:
    break;
  }
  return 0;
}

void init_TAGE()
{
  int base_predictor_entries = 1 << tagepcBits;
  int tagged_predictor_entries = 1 << tageIndexBits;
  T_0 = (uint8_t *)malloc(base_predictor_entries * sizeof(uint8_t));
  T_1 = (tagged_predictor *)malloc(base_predictor_entries * sizeof(tagged_predictor));
  T_2 = (tagged_predictor *)malloc(base_predictor_entries * sizeof(tagged_predictor));
  T_3 = (tagged_predictor *)malloc(base_predictor_entries * sizeof(tagged_predictor));
  T_4 = (tagged_predictor *)malloc(base_predictor_entries * sizeof(tagged_predictor));
  int i = 0;
  for (i = 0; i < base_predictor_entries; i++)
  {
    T_0[i] = 4;
  }
  for (i = 0; i < tagged_predictor_entries; i++)
  {
    T_1[i].pred = 4;
    T_2[i].pred = 4;
    T_3[i].pred = 4;
    T_4[i].pred = 4;
    T_1[i].tag = 0;
    T_2[i].tag = 0;
    T_3[i].tag = 0;
    T_4[i].tag = 0;
    T_1[i].u = 0;
    T_2[i].u = 0;
    T_3[i].u = 0;
    T_4[i].u = 0;
  }
  history = 0;
}

uint8_t TAGE_predict(uint32_t pc)
{
  uint32_t base_predictor_entries = 1 << tagepcBits;
  uint32_t pc_lower_bits = pc & (base_predictor_entries - 1);
  uint32_t tagged_index_1 = index_hash(pc, history, 1);
  uint32_t tagged_index_2 = index_hash(pc, history, 2);
  uint32_t tagged_index_3 = index_hash(pc, history, 3);
  uint32_t tagged_index_4 = index_hash(pc, history, 4);
  uint32_t tagged_hash_1 = tag_hash(pc, history, 1);
  uint32_t tagged_hash_2 = tag_hash(pc, history, 2);
  uint32_t tagged_hash_3 = tag_hash(pc, history, 3);
  uint32_t tagged_hash_4 = tag_hash(pc, history, 4);
  uint8_t flag = 0;
  uint8_t number = 0;
  if (T_1[tagged_index_1].tag == tagged_hash_1)
  {
    flag = flag + 1;
  }
  if (T_2[tagged_index_2].tag == tagged_hash_2)
  {
    flag = flag + 2;
  }
  if (T_3[tagged_index_3].tag == tagged_hash_3)
  {
    flag = flag + 4;
  }
  if (T_4[tagged_index_4].tag == tagged_hash_4)
  {
    flag = flag + 8;
  }
  if ((flag & 8) != 0)
  {
    number = 4;
  }
  else if ((flag & 4) != 0)
  {
    number = 3;
  }
  else if ((flag & 2) != 0)
  {
    number = 2;
  }
  else if ((flag & 1) != 0)
  {
    number = 1;
  }
  else
  {
    number = 0;
  }
  switch (number)
  {
  case 0:
    if (T_0[pc_lower_bits] <= 3)
      return NOTTAKEN;
    else
      return TAKEN;
    break;
  case 1:
    if (T_1[tagged_index_1].pred <= 3)
      return NOTTAKEN;
    else
      return TAKEN;
    break;
  case 2:
    if (T_2[tagged_index_2].pred <= 3)
      return NOTTAKEN;
    else
      return TAKEN;
    break;
  case 3:
    if (T_3[tagged_index_3].pred <= 3)
      return NOTTAKEN;
    else
      return TAKEN;
    break;
  case 4:
    if (T_4[tagged_index_4].pred <= 3)
      return NOTTAKEN;
    else
      return TAKEN;
    break;
  
  default:
    return NOTTAKEN;
  }
}

void train_TAGE(uint32_t pc, uint8_t outcome)
{
  uint32_t base_predictor_entries = 1 << tagepcBits;
  uint32_t pc_lower_bits = pc & (base_predictor_entries - 1);
  uint32_t tagged_index_1 = index_hash(pc, history, 1);
  uint32_t tagged_index_2 = index_hash(pc, history, 2);
  uint32_t tagged_index_3 = index_hash(pc, history, 3);
  uint32_t tagged_index_4 = index_hash(pc, history, 4);
  uint32_t tagged_hash_1 = tag_hash(pc, history, 1);
  uint32_t tagged_hash_2 = tag_hash(pc, history, 2);
  uint32_t tagged_hash_3 = tag_hash(pc, history, 3);
  uint32_t tagged_hash_4 = tag_hash(pc, history, 4);
  uint8_t flag = 0;
  uint8_t provider = 0;
  uint8_t altpred = 0;
  if (T_1[tagged_index_1].tag == tagged_hash_1)
  {
    flag = flag + 1;
  }
  if (T_2[tagged_index_2].tag == tagged_hash_2)
  {
    flag = flag + 2;
  }
  if (T_3[tagged_index_3].tag == tagged_hash_3)
  {
    flag = flag + 4;
  }
  if (T_4[tagged_index_4].tag == tagged_hash_4)
  {
    flag = flag + 8;
  }
  if ((flag & 8) != 0)
  {
    provider = 4;
    if ((flag & 4) != 0)
      altpred = 3;
    else if ((flag & 2) != 0)
      altpred = 2;
    else if ((flag & 1) != 0)
      altpred = 1;
    else
      altpred = 0;
  }
  else if ((flag & 4) != 0)
  {
    provider = 3;
    if ((flag & 2) != 0)
      altpred = 2;
    else if ((flag & 1) != 0)
      altpred = 1;
    else
      altpred = 0;
  }
  else if ((flag & 2) != 0)
  {
    provider = 2;
    if ((flag & 1) != 0)
      altpred = 1;
    else
      altpred = 0;
  }
  else if ((flag & 1) != 0)
  {
    provider = 1;
    altpred = 0;
  }
  else
  {
    provider = 0;
    altpred = 0;
  }
  uint8_t prediction = NOTTAKEN;
  uint8_t alt_prediction = NOTTAKEN;
  switch (provider)
  {
  case 0:
    if (T_0[pc_lower_bits] <= 3)
      prediction = NOTTAKEN;
    else
      prediction = TAKEN;
    break;
  case 1:
    if (T_1[tagged_index_1].pred <= 3)
      prediction = NOTTAKEN;
    else
      prediction = TAKEN;
    break;
  case 2:
    if (T_2[tagged_index_2].pred <= 3)
      prediction = NOTTAKEN;
    else
      prediction = TAKEN;
    break;
  case 3:
    if (T_3[tagged_index_3].pred <= 3)
      prediction = NOTTAKEN;
    else
      prediction = TAKEN;
    break;
  case 4:
    if (T_4[tagged_index_4].pred <= 3)
      prediction = NOTTAKEN;
    else
      prediction = TAKEN;
    break;
  
  default:
    break;
  }

  switch (altpred)
  {
  case 0:
    if (T_0[pc_lower_bits] <= 3)
      alt_prediction = NOTTAKEN;
    else
      alt_prediction = TAKEN;
    break;
  case 1:
    if (T_1[tagged_index_1].pred <= 3)
      alt_prediction = NOTTAKEN;
    else
      alt_prediction = TAKEN;
    break;
  case 2:
    if (T_2[tagged_index_2].pred <= 3)
      alt_prediction = NOTTAKEN;
    else
      alt_prediction = TAKEN;
    break;
  case 3:
    if (T_3[tagged_index_3].pred <= 3)
      alt_prediction = NOTTAKEN;
    else
      alt_prediction = TAKEN;
    break;
  
  default:
    break;
  }

  if (alt_prediction != prediction)
  {
    switch (provider)
    {
    case 1:
      if (prediction == TAKEN)
      {
        if (T_1[tagged_index_1].u < 3)
          T_1[tagged_index_1].u = T_1[tagged_index_1].u + 1;
      }
      else
      {
        if (T_1[tagged_index_1].u > 0)
          T_1[tagged_index_1].u = T_1[tagged_index_1].u - 1;
      }
      break;
    case 2:
      if (prediction == TAKEN)
      {
        if (T_2[tagged_index_2].u < 3)
          T_2[tagged_index_2].u = T_2[tagged_index_2].u + 1;
      }
      else
      {
        if (T_2[tagged_index_2].u > 0)
          T_2[tagged_index_2].u = T_2[tagged_index_2].u - 1;
      }
      break;
    case 3:
      if (prediction == TAKEN)
      {
        if (T_3[tagged_index_3].u < 3)
          T_3[tagged_index_3].u = T_3[tagged_index_3].u + 1;
      }
      else
      {
        if (T_3[tagged_index_3].u > 0)
          T_3[tagged_index_3].u = T_3[tagged_index_3].u - 1;
      }
      break;
    case 4:
      if (prediction == TAKEN)
      {
        if (T_4[tagged_index_4].u < 3)
          T_4[tagged_index_4].u = T_4[tagged_index_4].u + 1;
      }
      else
      {
        if (T_4[tagged_index_4].u > 0)
          T_4[tagged_index_4].u = T_4[tagged_index_4].u - 1;
      }
      break;
    
    default:
      break;
    }
  }
  switch (provider)
  {
  case 0:
    if (outcome == TAKEN)
    {
      if (T_0[pc_lower_bits] < 7)
        T_0[pc_lower_bits] = T_0[pc_lower_bits] + 1;
    }
    else
    {
      if (T_0[pc_lower_bits] > 0)
        T_0[pc_lower_bits] = T_0[pc_lower_bits] - 1;
    }
    break;

  case 1:
    if (outcome == TAKEN)
    {
      if (T_1[tagged_index_1].pred < 7)
        T_1[tagged_index_1].pred = T_1[tagged_index_1].pred + 1;
    }
    else
    {
      if (T_1[tagged_index_1].pred > 0)
        T_1[tagged_index_1].pred = T_1[tagged_index_1].pred - 1;
    }
    break;

  case 2:
    if (outcome == TAKEN)
    {
      if (T_2[tagged_index_2].pred < 7)
        T_2[tagged_index_2].pred = T_2[tagged_index_2].pred + 1;
    }
    else
    {
      if (T_2[tagged_index_2].pred > 0)
        T_2[tagged_index_2].pred = T_2[tagged_index_2].pred - 1;
    }
    break;

  case 3:
    if (outcome == TAKEN)
    {
      if (T_3[tagged_index_3].pred < 7)
        T_3[tagged_index_3].pred = T_3[tagged_index_3].pred + 1;
    }
    else
    {
      if (T_3[tagged_index_3].pred > 0)
        T_3[tagged_index_3].pred = T_3[tagged_index_3].pred - 1;
    }
    break;
  
  case 4:
    if (outcome == TAKEN)
    {
      if (T_4[tagged_index_4].pred < 7)
        T_4[tagged_index_4].pred = T_4[tagged_index_4].pred + 1;
    }
    else
    {
      if (T_4[tagged_index_4].pred > 0)
        T_4[tagged_index_4].pred = T_4[tagged_index_4].pred - 1;
    }
    break;
    
  default:
    break;
  }
  history = ((history << 1) | outcome);
  uint32_t tagged_index_table[4];
  tagged_predictor* tagged_predictor_table[4];
  uint32_t tagged_hash_table[4];
  tagged_index_table[0] = tagged_index_1;
  tagged_index_table[1] = tagged_index_2;
  tagged_index_table[2] = tagged_index_3;
  tagged_index_table[3] = tagged_index_4;
  tagged_predictor_table[0] = T_1;
  tagged_predictor_table[1] = T_2;
  tagged_predictor_table[2] = T_3;
  tagged_predictor_table[3] = T_4;
  tagged_hash_table[0] = tagged_hash_1;
  tagged_hash_table[1] = tagged_hash_2;
  tagged_hash_table[2] = tagged_hash_3;
  tagged_hash_table[3] = tagged_hash_4;
  if ((outcome != prediction) && (provider < 4))
  {
    uint8_t j = 0;
    for (j = provider+1; j <= 4; j++)
    {
      uint32_t tagged_index = tagged_index_table[j-1];
      tagged_predictor* tp = tagged_predictor_table[j-1];
      uint32_t tagged_hash = tagged_hash_table[j-1];
      if (tp[tagged_index].u == 0)
      {
        tp[tagged_index].u = 0;
        tp[tagged_index].tag = (uint8_t)tagged_hash;
        tp[tagged_index].pred = 4;
        break;
      }
    }
    if (j == 5)
    {
      for (j = provider+1; j <= 4; j++)
      {
        uint32_t tagged_index = tagged_index_table[j-1];
        tagged_predictor* tp = tagged_predictor_table[j-1];
        tp[tagged_index].u = tp[tagged_index].u - 1;
      }
    }
  }

}

void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
    break;
  case GSHARE:
    init_gshare();
    break;
  case TOURNAMENT:
    init_tournament();
    break;
  case CUSTOM:
    init_TAGE();
    break;
  default:
    break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint32_t make_prediction(uint32_t pc, uint32_t target, uint32_t direct)
{

  // Make a prediction based on the bpType
  switch (bpType)
  {
  case STATIC:
    return TAKEN;
  case GSHARE:
    return gshare_predict(pc);
  case TOURNAMENT:
    return tournament_predict(pc);
  case CUSTOM:
    return TAGE_predict(pc);
  default:
    break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint32_t target, uint32_t outcome, uint32_t condition, uint32_t call, uint32_t ret, uint32_t direct)
{
  if (condition)
  {
    switch (bpType)
    {
    case STATIC:
      return;
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tournament(pc, outcome);
    case CUSTOM:
      return train_TAGE(pc,outcome);
    default:
      break;
    }
  }
}
