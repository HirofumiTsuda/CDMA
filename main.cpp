#include <iostream>
#include <complex>
#include <cmath>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <omp.h>
#include <algorithm>
#include <functional>


#define CHIP 31
#define BIT 1000
#define MIN_USER_NUM 4
#define MAX_USER_NUM CHIP
#define INTERVAL 20
#define LATE CHIP*10*INTERVAL
#define CHANNEL_LENGTH (CHIP*BIT)*INTERVAL+LATE
#define COV_COUNT 1
#define USER_PLUS 2
#define MAX_FADING_NUM 0
#define MAX_DURATION 3
#define FADING_POWER -15 //(db)

#define SNR 25

#define TRIAL_NUMBER 10000
#include "correlation.hpp"
#include "code.hpp"

using namespace std;

double getSection(double *, int);
double getRandom();

void set_M(int *);
void set_Gold_element(int *);
void setLate(int *, int);
void setBit(int *);
void initChannel();
void setPhase(complex<double> *);
double getBER(int *, int *);
void setFading(int *,int a[MAX_USER_NUM][MAX_FADING_NUM]);
void setBitToChannel(int *, complex<double> *, int,complex<double>,int,int *);
void restoreBit(std::complex<double> *, int *, int ,complex<double>);
void quicksort(double *a, int , int );
double normCovariance(std::vector<double>);
double norm(double, double);
double covariance(std::vector<double>);
double average(std::vector<double> );
double reciprocalSum(std::vector<double> );
std::vector<double> setInit(int, std::vector<int>);
std::vector<double> setInitOrder(int, std::vector<int>);
std::vector<double> setInitEqual(int, std::vector<int>);
std::vector<double> setInitPerfectEqual(int,double);
std::vector<double> setInitPerfectEqualMax(int,double);
std::vector<double> setInitPerfectEqualInterval(int,double);
int same(bool *,int);

//通信路
complex<double> channel[CHANNEL_LENGTH];

int main(void) {
  complex<double> code[MAX_USER_NUM][CHIP];
  complex<double> phase[MAX_USER_NUM];
  int bit[MAX_USER_NUM][BIT];
  int restore[MAX_USER_NUM][BIT];
  int late[MAX_USER_NUM];
  int M[MAX_USER_NUM]; 
  int gold_element[CHIP];
  int fading_num[MAX_USER_NUM];
  int fading_late[MAX_USER_NUM][MAX_FADING_NUM];
  std::vector<int> pNum;
  std::vector<double> initVector;
  int pnumInt;
  char space;
  ifstream ifs("pnum.dat");
  ofstream ofs("../data/IT/cheby_LSF.dat");
  while (!ifs.eof()) {
    ifs >> pnumInt;
    pNum.push_back(pnumInt);
  }
#ifdef _OPENMP
  cout << "ok" << endl;
#else
  cout << "no" << endl;
#endif

#ifdef _OPENMP
  cout << "使用可能なスレッドの最大数" <<  omp_get_max_threads() << endl;

#endif

#pragma omp parallel
  {
    cout << "start" << endl;
  }

  int user;
  int count = 0;
  double ber;
  double berSum;
  srand((unsigned) time(NULL));
  //ofs <<  "#ave" << " " << "normVar" << " " <<  "var"  << " " <<  "reciprocalsum" << " " <<"ber" << endl;
  ofs << "#user         ber"  << endl;
  for (user = MIN_USER_NUM; user < MAX_USER_NUM+1; user=user+USER_PLUS) {

    cout << "user:" << user << endl;

    for (int cov_num = 0; cov_num < COV_COUNT; cov_num++) {
      berSum = 0.0;
      //距離の分散をゲット
      double normVar = normCovariance(initVector);
      double var = covariance(initVector);
      double ave = average(initVector);
      double reciprocal = reciprocalSum(initVector);


      for (count = 0; count < TRIAL_NUMBER; count++) {
	//通信路を初期化
	initChannel();
	//初期値をゲット
	initVector = setInitPerfectEqualInterval(user, 1.0/(2.0*MAX_USER_NUM));
	set_M(M);
	set_Gold_element(gold_element);
	//符号とビットと位相と 遅延をセット
	setLate(late, user);
	setPhase(phase);
	setFading(fading_num,fading_late);
#pragma omp parallel for
	for (int i = 0; i < user; i++) {
	  setBit(bit[i]);
	  //weyl符号
	  //gold_weyl(code[i], initVector.at(i), i,CHIP,1023);
	  //half_weyl_chaos(code[i],i,1023,initVector.at(i),CHIP);
	  //weyl(code[i],initVector.at(i),CHIP);
	  //Gold符号
	  //gold(code[i],i,CHIP,CHIP);
	  //chaos符号
	  chaos(code[i],2,mod1(sqrt(pNum.at(gold_element[i]))),CHIP);
	  //cheby_opt(code[i],std::sqrt(3)-2,2,mod1(sqrt(pNum.at(i))),CHIP);
	  LSF(code[i],sqrt(3)-2.0,CHIP);
	  //FZC sequence
	  //FZC(code[i],M[i],1.0,1.0,1.275,CHIP);
	  //sp sequence
	  //spSequence(code[i],M[i],CHIP);
	  //通信路にセット
	  setBitToChannel(bit[i], code[i], late[i], phase[i],fading_num[i],fading_late[i]);
	}
	//すべてのデータがセットされたので順にとりだしていく
#pragma omp parallel for reduction(+:berSum)
	for (int i = 0; i < user; i++) {
	  restoreBit(code[i], restore[i], late[i],phase[i]);
	  //ber = getBER(restore[i], bit[i]);
	  berSum += getBER(restore[i], bit[i]);
	  //cout << "USER:" <<  user << ",BER:" << ber << endl;
	}

      }

      //誤り率がでてくる
      //ユーザー数×試行回数でわる
      berSum = berSum / (TRIAL_NUMBER * user);
      cout << "ber:" << berSum << endl;
      //ofs <<  ave << " " << normVar << " " <<  var  << " " << reciprocal << " " <<  berSum << endl;
      ofs << user << " " << berSum << endl;
    }

  }

  ifs.close();
  ofs.close();

  return 0;
}

double getRandom() {
  return (double)rand() / (RAND_MAX);
}

void set_M(int *M){
  for(int k=0;k<MAX_USER_NUM;k++){
    M[k] = (k+1);
  }
  for(int i=0;i<MAX_USER_NUM-1;i++){
    int j = rand()%(MAX_USER_NUM-1);
    int t = M[i];
    M[i] = M[j];
    M[j] = t;
  }
}

void set_Gold_element(int *element){
  for(int k=0;k<CHIP;k++){
    element[k] = k;
  }
  for(int i=0;i<CHIP;i++){
    int j = rand()%(CHIP);
    int t = element[i];
    element[i] = element[j];
    element[j] = t;
  }
}

double getSection(double *data, int length) {
  bool flag = true;
  double k = 0.0;
  double delta = 0.001;
  //下から切片をずらしていって交差しなくなる最小のkを求める
  for (k = delta;; k = k + delta) {
    flag = true;
    //cout << "k:" << k << endl;
    for (int i = 1; i <= length; i++) {
      if (-log(i) + k < log(data[i - 1])) {
	flag = false;
	break;
      }
    }

    if (flag)
      break;
  }
  return exp(k);
}

void setFading(int *fad_num,int fad_late[MAX_USER_NUM][MAX_FADING_NUM]){
  for(int k=0;k<MAX_USER_NUM;k++){
    fad_num[k] = (int)(MAX_FADING_NUM * getRandom());
  }
  for(int k=0;k<MAX_USER_NUM;k++){
    for(int n=0;n<fad_num[k];k++){
      fad_late[k][n] = (int)(MAX_DURATION*CHIP*INTERVAL * getRandom());
    }
  }
}

void setLate(int *lateSet, int num) {
  for (int i = 0; i < num; i++)
    lateSet[i] = (int)(LATE * getRandom());
}

void setBit(int *bit) {
  for (int i = 0; i < BIT; i++) {
    if (getRandom() >= 0.5) {
      bit[i] = 1;
    } else {
      bit[i] = -1;
    }
  }
}

void setPhase(complex<double> *phase){
  std::complex<double> I(0.0,1.0);
  for (int n=0;n < MAX_USER_NUM; n++){
    phase[n] = std::exp(2.0*M_PI*I*getRandom()); 
  }
}

void initChannel() {
  for (int i = 0; i < CHANNEL_LENGTH; i++) {
    channel[i] = complex<double>(0.0, 0.0);
  }
}

double getBER(int *estimate, int *correct) {
  int error = 0;
  for (int i = 0; i < BIT; i++) {
    //cout << "bit:" << correct[i] << ",estimate:" << estimate[i] << endl;
    if (estimate[i] != correct[i])
      error++;
  }
  return ((double)error) / BIT;
}

void setBitToChannel(int *bit, complex<double> *code, int late, complex<double> phase,int fad_num,int *fad_time) {
  double power = pow(10.0,FADING_POWER/20);
  std::complex<double> I(0.0,1.0);
  for (int i = 0; i < BIT; i++) {
    for (int j = 0; j < CHIP; j++) {
      for (int k = 0; k < INTERVAL; k++) {
	channel[late + INTERVAL * (i * CHIP + j) + k] += ((double)bit[i]) * code[j]*phase;
      }
    }
  }
  // FADING
  for (int i = 0; i < BIT; i++) {
    for (int j = 0; j < CHIP; j++) {
      for(int l=0;l<fad_num;l++){
	for (int k = 0; k < INTERVAL; k++) {
	  channel[late + fad_time[l] + INTERVAL * (i * CHIP + j) + k] += power*std::exp(2.0*M_PI*I*getRandom())*((double)bit[i]) * code[j]*phase;
	}
      }
    }
  }
}

void restoreBit(std::complex<double> *code, int *restore, int late,complex<double> phase) {
  match_noise(code, channel, restore, CHIP, BIT, INTERVAL, late,phase,SNR);
}

double normCovariance(std::vector<double> pNumSqrt) {
  double ave = 0;
  double var = 0;
  int length = pNumSqrt.size();
  //まずソートする
  std::sort(pNumSqrt.begin(), pNumSqrt.end());
  //まず平均をだす
  for (int i = 0; i < length; i++) {
    ave = ave + norm(pNumSqrt.at(i), pNumSqrt.at((i + 1) % length));
  }
  ave = ave / length;
  //分散をだす
  for (int i = 0; i < length; i++) {
    var = var + pow((ave - norm(pNumSqrt.at(i), pNumSqrt.at((i + 1) % length))), 2);
  }
  var = var / length;

  return var;

}

double reciprocalSum(std::vector<double> pNumSqrt) {
  double ave = 0;
  double var = 0;
  int length = pNumSqrt.size();
  //まずソートする
  cout << "length:"<< length << endl;
  std::sort(pNumSqrt.begin(), pNumSqrt.end());
  double sum = 0.0;
  //逆数の和をとる
  for (int i = 0; i < length; i++) {
    sum = sum + 1/norm(pNumSqrt.at(i), pNumSqrt.at((i + 1) % length));
  }

  return sum;

}


double covariance(std::vector<double> v){

  double ave =0,var=0;
  int length = v.size();
  //まず平均をだす
  for (int i = 0; i < length; i++) {
    ave = ave + v.at(i);
  }
  ave = ave / length;
  //分散をだす
  for (int i = 0; i < length; i++) {
    var = var + pow(ave - v.at(i), 2);
  }
  var = var / length;

  return var;
}

double average(std::vector<double> v){
  double ave = 0.0;
  int length = v.size();
  for(int i=0;i<length;i++){
    ave = ave + v.at(i);
  }
  return ave/length;
}

std::vector<double> setInit(int user, std::vector<int> pNum) {
  std::vector<double> initVector;
  bool flag;
  std::vector<int> alreadyChoose;
  for (int i = 0; i < user;) {
    flag = true;
    int num = (int)(getRandom() * pNum.size());
    for (int j = 0; j < alreadyChoose.size(); j++) {
      if (num == alreadyChoose.at(j)) {
	flag = false;
	break;
      }
    }
    if (flag) {
      alreadyChoose.push_back(num);
      initVector.push_back(mod1(sqrt(pNum.at(num))));
      i++;
    }
  }
  return initVector;
}

std::vector<double> setInitOrder(int user, std::vector<int> pNum) {
  std::vector<double> initVector;
  for(int i=0;i<user;i++){
    initVector.push_back(mod1(sqrt(pNum.at(i))));
  }
  return initVector;
}

std::vector<double> setInitEqual(int user, std::vector<int> pNum) {
  std::vector<double> initVector;
  bool flag;
  double element;
  std::vector<int> alreadyChoose;
  for (int i = 0; i < user;) {
    flag = true;
    int num = (int)(getRandom() * pNum.size());
    for (int j = 0; j < alreadyChoose.size(); j++) {
      if (num == alreadyChoose.at(j)) {
	flag = false;
	break;
      }
    }
    if (flag) {
      alreadyChoose.push_back(num);
      element = mod1(((double)i)/user + mod1(sqrt(num))/(2*user));
      initVector.push_back(element);
      i++;
    }
  }
  return initVector;
}

std::vector<double> setInitPerfectEqual(int user,double init) {
  std::vector<double> initVector;
  double element;
  for(int i=0;i<user;i++){
    element = mod1(((double) i)/user + init);
    initVector.push_back(element);
  }
  return initVector;
}

double norm(double x, double y) {
  if (x < y) {
    return (1 + x - y > y - x) ? y - x : 1 + x - y;
  } else {
    return (1 + y - x > x - y) ? x - y : 1 + y - x;
  }
}

std::vector<double> setInitPerfectEqualMax(int user, double init) {
  std::vector<double> initVector;
  bool flag;
  double element;
  std::vector<int> alreadyChoose;
  for (int i = 0; i < user;) {
    flag = true;
    int num = (int)(getRandom() * MAX_USER_NUM);
    for (int j = 0; j < alreadyChoose.size(); j++) {
      if (num == alreadyChoose.at(j)) {
	flag = false;
	break;
      }
    }
    if (flag) {
      alreadyChoose.push_back(num);
      element = mod1(init + ((double)num)/MAX_USER_NUM);
      initVector.push_back(element);
      i++;
    }
  }
  return initVector;
}

std::vector<double> setInitPerfectEqualInterval(int user,double init){
  std::vector<double> initVector;
  int n = user;
  for(int i=1;i<user+1;i++){
    n = i;
    double vdc = 0, denom = 1;
    while(n){
      vdc += fmod(n, 2.0) / (denom *= 2.0);
      n /= 2.0; // note: conversion from 'double' to 'int'
    }
    initVector.push_back(mod1(vdc+init));
  }
  return initVector;
}

int same(bool *box,int number){
  bool flag = 1;
  int second;
  for(int i=0;i<number/2;i++){
    if(box[i] != box[number/2 + i])
      return number/2+i;
  }

  if(number==2){
    for(second=0;second<MAX_USER_NUM;second++){
      if(box[second] == 1){
	return second/2;
      }
    }
  }
  return same(box,number/2);

}
