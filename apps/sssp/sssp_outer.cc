/*
    Distributed Under the MIT license
    Uses the Bellman-Ford/Dijkstra Algorithm to find shortest path distances
    Programs by Masab Ahmad (UConn)
*/

#include <cstdio>
#include <cstdlib>
#include <pthread.h>
//#include "carbon_user.h"    /*For the Graphite Simulator*/
#include <time.h>
#include <sys/timeb.h>

#define MAX            100000000
#define INT_MAX        100000000
#define BILLION 1E9

//Thread Argument Structure
typedef struct
{
   int*      local_min;
   int*      global_min;
   int*      Q;
   int*      D;
   int*      D_temp;
   int**     W;
   int**     W_index;
   int*      d_count;
   int       tid;
   int       P;
   int       N;
   int       DEG;
   pthread_barrier_t* barrier;
} thread_arg_t;

//Function Initializers
int initialize_single_source(int* D, int* D_temp, int* Q, int source, int N);
void relax(int u, int i, volatile int* D, int** W, int** W_index, int N);
int get_local_min(volatile int* Q, volatile int* D, int start, int stop, int N, int** W_index, int** W, int u);
void init_weights(int N, int DEG, int** W, int** W_index);

//Global Variables
int min = INT_MAX;
int min_index = 0;
pthread_mutex_t lock;
pthread_mutex_t locks[2097152]; //change the number of locks to approx or greater N
int u = -1;
int local_min_buffer[1024];
int global_min_buffer;
int terminate = 0;
int cntr = 0;
int *exist;
int *id;
int P_max=64;
thread_arg_t thread_arg[1024];
pthread_t   thread_handle[1024];

//Primary Parallel Function
void* do_work(void* args)
{
   volatile thread_arg_t* arg = (thread_arg_t*) args;

   int tid                  = arg->tid;      //thread id
   int P                    = arg->P;        //Max threads
   int* D_temp              = arg->D_temp;   //Temporary Distance Array
   int* D                   = arg->D;        //distabces
   int** W                  = arg->W;        //edge weights
   int** W_index            = arg->W_index;  //graph structure
   const int N              = arg->N;        //Max vertices
   const int DEG            = arg->DEG;      //edges per vertex
   int v = 0;

   int cntr_0 = 0;
   int neighbor=0;

   int start = static_cast<long long>(tid) * N / P;
   int stop = (tid + 1LL) * N / P;
   
   //printf("\n %d %d %d",tid,start,stop);

   pthread_barrier_wait(arg->barrier);

   while(terminate==0)
   {
         //printf("\n Start:%d",tid);
         for(v=start;v<stop;v++)
         {
           D[v] = D_temp[v];
         }

         pthread_barrier_wait(arg->barrier);

         terminate = 1;

         for(v=start;v<stop;v++)
         {

            if(exist[v]==0)
               continue;

            for(int i = 0; i < DEG; i++)
            {
               //if(v<N)
                  neighbor = W_index[v][i];

               if(neighbor>=N)
                  break;

               //pthread_mutex_lock(&locks[neighbor]);

               //relax
               if((D[W_index[v][i]] > (D[v] + W[v][i])))    //relax, update distance
                  D_temp[W_index[v][i]] = D[v] + W[v][i];
               //printf("\n %d",D_temp[W_index[v][i]]);
               //pthread_mutex_unlock(&locks[neighbor]);
            }
         }
         
         pthread_barrier_wait(arg->barrier);

         for(v=start;v<stop;v++)
         {
           if(D[v] != D_temp[v]) 
           {
             terminate = 0;
           }
         }

         pthread_barrier_wait(arg->barrier);
         cntr_0++; 
   }
   //printf("\n terminate  %d",tid);

   if(tid==0)
     cntr = cntr_0;

   pthread_barrier_wait(arg->barrier);

   return NULL;
}

// Create a dotty graph named 'fn'.
void make_dot_graph(int **W,int **W_index,int *exist,int *D,int N,int DEG,const char *fn)
{
   FILE *of = fopen(fn,"w");

   fprintf (of,"digraph D {\n"
         "  rankdir=LR\n"
         "  size=\"4,3\"\n"
         "  ratio=\"fill\"\n"
         "  edge[style=\"bold\"]\n"
         "  node[shape=\"circle\",style=\"filled\"]\n");

   // Write out all edges.
   for (int i = 0; i != N; ++i) {
      if (exist[i]) {
         for (int j = 0; j != DEG; ++j) {
            if (W_index[i][j] != INT_MAX) {
               fprintf (of,"%d -> %d [label=\"%d\"]\n",i,W_index[i][j],W[i][j]);
            }
         }
      }
   }

# ifdef DISTANCE_LABELS
   // We label the vertices with a distance, if there is one.
   fprintf (of,"0 [fillcolor=\"red\"]\n");
   for (int i = 0; i != N; ++i) {
      if (D[i] != INT_MAX) {
         fprintf (of,"%d [label=\"%d (%d)\"]\n",i,i,D[i]);
      }
   }
# endif

   fprintf (of,"}\n");

   fclose (of);
}

int main(int argc, char** argv)
{
   int N = 0;
   int DEG = 0;
   FILE *file0 = NULL;

   const int select = atoi(argv[1]);
   const int P = atoi(argv[2]);
   if(select==0)
   {
      N = atoi(argv[3]);
      DEG = atoi(argv[4]);
      printf("\nGraph with Parameters: N:%d DEG:%d\n",N,DEG);
   }

   if(select==1)
   {
      const char *filename = argv[3];
      file0 = fopen(filename,"r");
      N = 2000000;  //can be read from file if needed, this is a default upper limit
      DEG = 16;     //also can be reda from file if needed, upper limit here again
   }

   int lines_to_check=0;
   char c;
   int number0;
   int number1;
   int previous_node = -1;
   int inter = -1;

   int* D;
   int* D_temp;
   int* Q;

   D = (int*) malloc(N * sizeof(int));
   D_temp = (int*) malloc(N * sizeof(int));
   Q = (int*) malloc(N * sizeof(int));
   exist = (int*) malloc(N * sizeof(int));
   id = (int*) malloc(N * sizeof(int));
   int d_count = N;
   pthread_barrier_t barrier;

   int** W = (int**) malloc(N*sizeof(int*));
   int** W_index = (int**) malloc(N*sizeof(int*));
   for(int i = 0; i < N; i++)
   {
      W[i] = (int*) malloc(DEG*sizeof(int));
      W_index[i] = (int*) malloc(DEG*sizeof(int));
   }

   for(int i=0;i<N;i++)
   {
      for(int j=0;j<DEG;j++)
      {
         W[i][j] = INT_MAX;
         W_index[i][j] = INT_MAX;
      }
      exist[i]=0;
      id[0] = 0;
   }

   if(select==1)
   {
      for(c=getc(file0); c!=EOF; c=getc(file0))
      {
         if(c=='\n')
            lines_to_check++;

         if(lines_to_check>3)
         {   
            int read_count = fscanf(file0, "%d %d", &number0,&number1);
            (void)read_count;
            //printf("\n%d %d",number0,number1);

            exist[number0] = 1; exist[number1] = 1;
            id[number0] = number0;
            if(number0==previous_node) {
               inter++;
            } else {
               inter=0;
            }

            // We don't support parallel edges, so check for that and ignore.
            bool exists = false;
            for (int i = 0; i != inter; ++i) {
               if (W_index[number0][i] == number1) {
                  exists = true;
                  break;
               }
            }

            if (!exists) {
               W[number0][inter] = inter+1;
               W_index[number0][inter] = number1;
               previous_node = number0;
            }
         }   
      } //W[2][0] = -1;
   }

   //Generate a uniform random graph
   if(select==0)
   {
      init_weights(N, DEG, W, W_index);
   }

   //Synchronization Variables
   pthread_barrier_init(&barrier, NULL, P);
   pthread_mutex_init(&lock, NULL);
   for(int i=0; i<N; i++)
   {
      pthread_mutex_init(&locks[i], NULL);
      if(select==0)
         exist[i]=1;
   } 

   //Initialize data structures
   initialize_single_source(D, D_temp, Q, 0, N);

   //Thread Arguments
   for(int j = 0; j < P; j++) {
      thread_arg[j].local_min  = local_min_buffer;
      thread_arg[j].global_min = &global_min_buffer;
      thread_arg[j].Q          = Q;
      thread_arg[j].D          = D;
      thread_arg[j].D_temp     = D_temp;
      thread_arg[j].W          = W;
      thread_arg[j].W_index    = W_index;
      thread_arg[j].d_count    = &d_count;
      thread_arg[j].tid        = j;
      thread_arg[j].P          = P;
      thread_arg[j].N          = N;
      thread_arg[j].DEG        = DEG;
      thread_arg[j].barrier    = &barrier;
   }

   //for clock time
   struct timespec requestStart, requestEnd;
   clock_gettime(CLOCK_REALTIME, &requestStart);

   // Enable Graphite performance and energy models
   //CarbonEnableModels();

   //create threads
   for(int j = 0; j < P; j++) {
      pthread_create(thread_handle+j,
            NULL,
            do_work,
            (void*)&thread_arg[j]);
   }

   //join threads
   for(int j = 0; j < P; j++) { //mul = mul*2;
      pthread_join(thread_handle[j],NULL);
   }

   // Disable Graphite performance and energy models
   //CarbonDisableModels();

   //read clock for time
   clock_gettime(CLOCK_REALTIME, &requestEnd);
   double accum = ( requestEnd.tv_sec - requestStart.tv_sec ) + ( requestEnd.tv_nsec - requestStart.tv_nsec ) / BILLION;
   printf( "Elapsed time: %lfs\n", accum );

   //printf("\ndistance:%d \n",D[N-1]);

   make_dot_graph(W,W_index,exist,D,N,DEG,"rgraph.dot");

   printf("\n Iterations taken:%d",cntr);

   //for distance values check
   FILE * pfile;
   pfile = fopen("myfile.txt","w");
   fprintf (pfile,"distances:\n");
   for(int i = 0; i < N; i++) {
      if(D[i] != INT_MAX) {
         fprintf(pfile,"distance(%d) = %d\n", i, D[i]);
      }
   }
   fclose(pfile);
   printf("\n");

   return 0;
}

int initialize_single_source(int*  D,
      int*  D_temp,
      int*  Q,
      int   source,
      int   N)
{
   for(int i = 0; i < N; i++)
   {
      D[i] = INT_MAX;
      D_temp[i] = INT_MAX;
      Q[i] = 1;
   }

   D[source] = 0;
   D_temp[source] = 0;
   return 0;
}

int get_local_min(volatile int* Q, volatile int* D, int start, int stop, int N, int** W_index, int** W, int u)
{
   int min = INT_MAX;
   int min_index = N;

   for(int i = start; i < stop; i++) 
   {
      if(W_index[u][i]==-1 || W[u][i]==INT_MAX)
         continue;
      if(D[i] < min && Q[i]) 
      {
         min = D[i];
         min_index = W_index[u][i];
      }
   }
   return min_index;
}

void relax(int u, int i, volatile int* D, int** W, int** W_index, int N)
{
   if((D[W_index[u][i]] > (D[u] + W[u][i]) && (W_index[u][i]!=-1 && W_index[u][i]<N && W[u][i]!=INT_MAX)))
      D[W_index[u][i]] = D[u] + W[u][i];
}

void init_weights(int N, int DEG, int** W, int** W_index)
{
   // Initialize to -1
   for(int i = 0; i < N; i++)
      for(int j = 0; j < DEG; j++)
         W_index[i][j]= -1;

   // Populate Index Array
   for(int i = 0; i < N; i++)
   {
      int last = 0;
      for(int j = 0; j < DEG; j++)
      {
         if(W_index[i][j] == -1)
         {
            int neighbor = i + j;//rand()%(max);
            if(neighbor > last)
            {
               W_index[i][j] = neighbor;
               last = W_index[i][j];
            }
            else
            {
               if(last < (N-1))
               {
                  W_index[i][j] = (last + 1);
                  last = W_index[i][j];
               }
            }
         }
         else
         {
            last = W_index[i][j];
         }
         if(W_index[i][j]>=N)
         {
            W_index[i][j] = N-1;
         }
      }
   }

   // Populate Cost Array
   for(int i = 0; i < N; i++)
   {
      for(int j = 0; j < DEG; j++)
      {
         double v = drand48();
         /*if(v > 0.8 || W_index[i][j] == -1)
           {       W[i][j] = MAX;
           W_index[i][j] = -1;
           }

           else*/ if(W_index[i][j] == i)
         W[i][j] = 0;

         else
            W[i][j] = (int) (v*100) + 1;
         //printf("   %d  ",W_index[i][j]);
      }
      //printf("\n");
   }
}
