#include <iostream>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <poll.h>
#include "threadfuns.h"
#include "utils.h"

//sunarthseis twn threads!
//dokimastikh
void hello(){
  st.cs_start();
  std::cout << "Quanmiong\n i am thread " << std::to_string(pthread_self()) << "\n";
  //st.print_safe(tprnt.c_str());
  st.cs_end();
  //std::cout << tprnt;
}

//GIA KYKLIKO BUFFER DIAFANEIWN KOU. NTOULA
//constructor p kanei initialize
pool::pool(int sz){
  fds = new tuple[sz]; //oso to bufferSize tha einai auto
  //memset(fds, 0, sizeof(fds)); //gemise to me 0 arxika
  start = 0;
  end = -1;
  count = 0;
  size = sz;
  lock = PTHREAD_MUTEX_INITIALIZER;
  nonempty = PTHREAD_COND_INITIALIZER;
  nonfull = PTHREAD_COND_INITIALIZER;
}
//akoloy8ei logikh diafaneiwn kou. Ntoula se producers consumers
void pool::place(tuple fd){
  pthread_mutex_lock(&lock) ;
  while(count >= size){
    //printf(">> Found Buffer Full \n");
    pthread_cond_wait (&nonfull , &lock) ;
  }
  end = (end + 1) % size ;
  fds[end] = fd ;
  count++;
  pthread_mutex_unlock(&lock) ;
}//telos sunarthshs

tuple pool::obtain(){
  tuple data;
  pthread_mutex_lock(&lock) ;
  while(count <= 0){
    //printf (">> Found Buffer Empty\n");
    pthread_cond_wait(&nonempty , &lock) ;
  }
  data = fds[start];
  //printf("Eimai o %d kai tsimphsa to fd %d\n", pthread_self(), data);
  start = (start + 1) % size ;
  count--;
  //std::cout << "eimai o " << pthread_self() << " phra" << data.fd << data.type << "\n";
  pthread_mutex_unlock(&lock) ;
  return data ;
}

//destructor
pool::~pool(){
  delete[] fds;
  pthread_cond_destroy(&nonempty);
  pthread_cond_destroy(&nonfull);
  pthread_mutex_destroy(&lock);
}


//SYNCHRO_STDOUT
synchro_stdout::synchro_stdout(){
  user = false;
  lock = PTHREAD_MUTEX_INITIALIZER;
  in_use = PTHREAD_COND_INITIALIZER;
}

synchro_stdout::~synchro_stdout(){
  user = false;
  pthread_cond_destroy(&in_use);
  pthread_mutex_destroy(&lock);
}

//arxh critical section gia printing sto stdout
void synchro_stdout::cs_start(){
  pthread_mutex_lock(&lock);
  while(user)
    pthread_cond_wait(&in_use, &lock);
  user =true;
}

//telos critical secton gia priting sto stdout
void synchro_stdout::cs_end(){
  user=false;
  pthread_mutex_unlock(&lock);
  pthread_cond_broadcast(&in_use);
}

//metadata poy krataei o server gia kathe worker
//tsekarw an exw auth th xwra
bool worker::has_country(std::string cntry ){
  for(int i=0; i<n_countries; i++)
    if(countries[i] == cntry)
      return true;
  return false;
}

void worker::add_country(std::string cntry ){
  if(has_country(cntry)) //h xwra yparxei, den thn ksanabazw
    return;
  std::string * newcountries = new std::string[n_countries+1];
  for(int i=0; i< n_countries; i++)
    newcountries[i] = countries[i];
  newcountries[n_countries] = cntry;
  n_countries++ ;
  delete[] countries;
  countries = newcountries;
}

//gia thn klash poy krataei workers kai th vlepoyn ta threads
worker_db::worker_db(){
  n_workers =0;
  workers = NULL;
  readers =0;
  writer = false;
  lock = PTHREAD_MUTEX_INITIALIZER;
  readcond = PTHREAD_COND_INITIALIZER;
  writecond = PTHREAD_COND_INITIALIZER;
}

worker_db::~worker_db(){
  for(int i=0; i< n_workers; i++)
    delete[] workers[i].countries;
  delete[] workers;
  pthread_cond_destroy(&readcond);
  pthread_cond_destroy(&writecond);
  pthread_mutex_destroy(&lock);
}

void worker_db::add_worker(worker wrkr){
  worker * newworkers = new worker[n_workers+1];
  for(int i=0; i< n_workers; i++)
    newworkers[i] = workers[i]; //thanks to operator overloading!
  newworkers[n_workers] = wrkr;
  n_workers++;
  delete[] workers;
  workers = newworkers;
}

//sthn arxh diabasmatos apo statistics port enhmerwnei th domh metadata gia workers
void worker_db::extract_worker(tuple sfd){
  uint16_t worker_port =0;
  read(sfd.fd, &worker_port, sizeof(worker_port));
  //std::cout << "Phra to " << ntohs(worker_port) << " " << ip <<"\n";
  int cntrs =0;
  receive_integer(sfd.fd, &cntrs);
  worker thisone;
  thisone.port = worker_port;
  thisone.address = sfd.address;
  std::string cntr;
  for(int j=0; j<cntrs; j++){
    receive_string(sfd.fd, &cntr, IO_PRM ); //pare xwra
    thisone.add_country(cntr);
  }
  add_worker(thisone);
}

//OI PARAKATW 4 METHODOI GIA DIABASMA K enhmerwsh THS WORKER_DB EINAI BASISMENA STO READERS/WRITER TWN DIAFANEIWN
//thelw na grapsw kati sth domh worker_db, gia na mpw se critical section
void worker_db::cs_writer_start(){
  pthread_mutex_lock(&lock);
  while(readers >0 || writer)
    pthread_cond_wait(&writecond, &lock);
  writer = true;
  pthread_mutex_unlock(&lock);
  //arxise to critcal section gia enhmerwsh ths classhs
}
//eksodos apo CS writer
void worker_db::cs_writer_end(){
  pthread_mutex_lock(&lock);
  writer = false;
  pthread_cond_broadcast(&readcond);
  pthread_cond_signal(&writecond);
  pthread_mutex_unlock(&lock);
}
//eisodos se CS gia reader apo thn klash
void worker_db::cs_reader_start(){
  pthread_mutex_lock(&lock);
  while(writer)
    pthread_cond_wait(&readcond, &lock);
  readers++ ;
  pthread_mutex_unlock(&lock);
  //arxise critical section gia diabasma apo thn classh
}
//eksodos apo CS gia reader
void worker_db::cs_reader_end(){
  pthread_mutex_lock(&lock);
  readers-- ;
  if(readers == 0)
    pthread_cond_signal(&writecond); //broadcast xwris if??
  pthread_mutex_unlock(&lock);
}

//gia anazhthsh worker basei xwras
worker * worker_db::search_worker_by_country(std::string country){
  for(int i=0; i<n_workers; i++)
    if(workers[i].has_country(country))
      return &(workers[i]); //gurna deikth ston worker
  return NULL;
}

//prepei na rwthsw olous tous workers?
bool must_ask_all(std::string quer){
  if(quer == "/diseaseFrequency1")
    return true;
  if(quer == "/searchPatientRecord")
    return true;
  if(quer == "/numPatientAdmissions1")
    return true;
  if(quer == "/numPatientDischarges1")
    return true;
  return false;
}

//rwtaw olous tous workers giati to erwthma den htan country-specific
void ask_them_all(int fd, std::string quest, int * fdsarr){
  //KSEXWRIZW POIO ERWTHMA EINAI GIA NA KSERW TI THA KANW
  work_db->cs_reader_start(); //critical. prosexei na mhn enhmerwnei kapoios th domh ekeinh thn wra
  fdsarr = new int[work_db->n_workers]; //gia na krathsw tous fds twn workers
  struct pollfd pollfds[work_db->n_workers];
  int already_read[work_db->n_workers]; //mh diabaseis ksana to idio paidi
  struct sockaddr_in * work_addresses = new struct sockaddr_in[work_db->n_workers] ; //gia na krataw plhrofories na kanw connect meta stous workers
  for(int i=0; i< work_db->n_workers; i++){
    //PAW NA FTIAKSW SOCKET GIA KATHE WORKER WSTE NA STEILW MHNYMA EKEI. STHN PORTA POY MOY EIXE ORISEI STHN ARXH
    work_addresses[i].sin_family = AF_INET;
    //inet_pton(AF_INET, serverIP, &(serv_addr.sin_addr)); //pare vale th dieu9unsh tou server
    work_addresses[i].sin_addr.s_addr = inet_addr(work_db->workers[i].address.c_str());
    work_addresses[i].sin_port = work_db->workers[i].port; //Vazw to port tou orismatos Serverport
    int work_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(work_sock < 0)
      {printf("socket error\n");}
    fdsarr[i] = work_sock;
    pollfds[i].fd = work_sock;
  }//telos for gia kathe worker
  int works_num = work_db->n_workers;
  work_db->cs_reader_end(); //telos critical diabasmatos apo th domh
  //dhmiourgw sundeseis gia na mporw na grafw/diabazw apo tous fds twn workers
  for(int i=0; i< works_num; i++)
    if(connect(fdsarr[i], (struct sockaddr *)&work_addresses[i], sizeof(work_addresses[i])) < 0)
      {printf("\nConnection to worker failed\n");pthread_exit(NULL);}

  //pame na tous metadwsoume ta erwthmata
  if(quest == "/diseaseFrequency1"){
    std::string disease; std::string date1; std::string date2;
    //diabase tis times twn orismatwn
    receive_string(fd, &disease ,IO_PRM);
    receive_string(fd, &date1 ,IO_PRM);
    receive_string(fd, &date2 ,IO_PRM);
    //prepei na rwthsw olous tous workers giati den exoume parametro country
    for(int i=0; i< works_num; i++){
      send_string(fdsarr[i], "/diseaseFrequency1" ,IO_PRM);
      send_string(fdsarr[i], &disease ,IO_PRM);
      send_string(fdsarr[i], &date1 ,IO_PRM);
      send_string(fdsarr[i], &date2 ,IO_PRM);
    }
  }//telos if diseaseFrequency1
  //PAW NA DIABASW APANTHSEIS
  //pare apanthsh
  int intreader=0;
  int intreader2=0;
  int kids_read =0;
  memset(already_read, 0, sizeof(already_read)); // arxika ola adiabasta
  while(kids_read < works_num){
    //arxikopoihsh se kathe loupa gia thn poll
    reset_poll_parameters(pollfds, works_num);
    int rc = poll(pollfds, works_num, 2000); //kanw poll
    if(rc == 0)
      {;;/*std::cout << "timeout\n";*/}
    else{ //tsekarw poioi einai etoimoi
      for(int i=0; i<works_num; i++){
        if((pollfds[i].revents == POLLIN) && (already_read[i] == 0)){ //1os diathesimos poy den exei diabastei
          receive_integer(pollfds[i].fd, &intreader);
          intreader2 += intreader;
          already_read[i] = 1;
          kids_read++;
        } //telos if diatheismothtas tou i
      } //telos for diathesimothtas olwn
    } //telos else timeout
  }//telos while gia poll
  std::cout << intreader2 << "\n";


  delete[] work_addresses;
}
