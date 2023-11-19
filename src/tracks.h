#include <stdio.h>

 float vol_1[64] = {1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,0,    1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,0,    1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,0,    1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,1,0 };
 float mel_1[64] = {0,0,0,0, 0,0,0,0, 0,1,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 0,1,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 0,1,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 0,1,0,0, 0,0,0,0 };
 float bas_1[64] = {0,0,0,0, 0,0,0,0, 1,1,1,1, 1,1,1,1,    3,3,3,3, 3,3,3,3, 1,1,1,1, 1,1,1,1,    0,0,0,0, 0,0,0,0, 1,1,1,1, 1,1,1,1,    3,3,3,3, 3,3,3,3, 1,1,1,1, 1,1,1,1 };

 float vol1[64] = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 };
 float mel1[64] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
 float bas1[64] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };

 float vol2[64] = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 };
 float mel2[64] = { 0,0,0,1, 1,1,3,3, 0,0,0,1, 1,1,3,3,    0,0,0,1, 1,1,3,3, 0,0,0,1, 1,1,3,3,    0,0,0,1, 1,1,3,3, 0,0,0,1, 1,1,3,3,    0,0,0,1, 1,1,3,3, 0,0,0,1, 1,1,3,3 };
 float bas2[64] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 3,3,3,3, 1,1,1,1,    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 3,3,3,3, 1,1,1,1 };

 float vol3[64] = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 };
 float mel3[64] = {0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,0,1,3,1};
 float bas3[64] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 1,1,1,1, 3,3,3,3,    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,    0,0,0,0, 0,0,0,0, 1,1,1,1, 3,3,3,3 };

 float vol4[64] = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 };
 float mel4[64] = { 0,-3,-1,0,	-3,-1,0,-3,	-1,0,-3,-1, 	0,-3,-1,0,	-3,-1,0,-3,	-1,0,-3,-1,	0,-3,-1,0,	-3,-1,0,-3,-1,	0,-3,-1,0,	-3,-1,0,-3,	-1,0,-3,-1,	0,-3,-1,0 };
 float bas4[64] = { -3,-3,-3,-3, -1,-1,-1,-1, -3,-3,-3,-3, -3,-3,-3,-3,    -3,-3,-3,-3, -1,-1,-1,-1, -3,-3,-3,-3, -3,-3,-3,-3,    -3,-3,-3,-3, -1,-1,-1,-1, -3,-3,-3,-3, -3,-3,-3,-3,    -3,-3,-3,-3, -1,-1,-1,-1, -3,-3,-3,-3, -3,-3,-3,-3 };

 float vol5[64] = { 1,0,0,0, 0,1,1,1, 1,0,1,0, 0,0,0,0,    0,0,0,0, 0,1,1,1,  1,0,1,0, 1,1,0,0,    1,0,0,0, 0,1,1,1,  1,0,1,0, 0,0,0,0,  0,0,0,0, 0,1,1,1,  1,0,1,1, 1,0,1,0  };
 float mel5[64] = { 0,0,0,0, 0,5,7,12, 8,0,7,0, 0,0,0,0,   0,0,0,0, 0,3,7,12, 8,0,7,0, 2,7,0,0,    0,0,0,0, 0,5,7,12, 8,0,7,0, 0,0,0,0,  0,0,0,0, 0,3,7,12, 8,0,7,0, 11,0,2,0 };
 float bas5[64] = { 0,0,0,0, 1,1,1,1, 0,0,0,0, 1,1,1,1,    0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0,    0,0,0,0, 1,1,1,1,  0,0,0,0, 1,1,1,1,  0,0,0,0, 0,0,0,0,  0,0,0,0, 0,0,0,0  };

 float vol6[64] = { 1,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,1,0,  1,0,0,1, 0,0,1,0,  1,0,0,0,    0,0,1,0,   1,0,0,1,    0,0,1,0,    1,0,0,0,     0,0,1,0,     1,0,0,1,     0,0,1,0  };
 float mel6[64] = { 0,0,0,0, 0,0,1,1, 0,0,0,0, 0,0,1,1, 0,0,0,0, 0,0,1,1,  0,0,0,1, 1,1,5,5, -2,-2,-2,-2, -2,-2,0,0, -2,-2,-2,0, 0,0,7,7,    -4,-4,-4,-4, -4,-4,-5,-5, 4,4,4,1,     1,1,-2,-2};
 float bas6[64] = { 0,0,0,0, 0,0,1,1, 0,0,0,0, 0,0,1,1, 0,0,0,0, 0,0,1,1,  0,0,0,0, 1,1,5,5, -2,-2,-2,-2, -2,-2,0,0, -2,-2,-2,-2, -2,-2,7,7, -4,-4,-4,-4, -4,-4,-4,-4, -5,-5,-2,-2, -2,-2,-2,-2};

 float vol7[64] = {1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0 };
 float mel7[64] = {8,7,5,0, 8,7,5,0, 8,7,5,0, 8,7,5,0, 8,7,5,0, 8,7,5,0, 8,7,5,0, 8,7,5,0, 7,6,4,0, 7,6,4,0, 7,6,4,0, 7,6,4,0, 7,6,4,0, 7,6,4,0, 7,6,4,0, 7,6,4,0};
 float bas7[64] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6,-6};

 float vol8[64] = {1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,1,0,1,0,1,0,0,0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,1,0,1,0,1,0,0,0};
 float mel8[64] = {0,0,0,0,0,0,-5,0,1,0,0,0,0,0,3,0,0,0,0,0,0,0,-5,0,1,0,-2,0,1,0,0,0,0,0,0,0,0,0,-5,0,1,0,0,0,0,0,3,0,0,0,0,0,0,0,-5,0,1,0,0,0,-2,0,0,0};
 float bas8[64] = {0,0,0,0,0,0,-1,-1,1,1,1,1,1,1,3,3,0,0,0,0,0,0,-5,-5,1,1,-2,-2,1,1,1,1,0,0,0,0,0,0,-5,-5,1,1,1,1,1,1,3,3,0,0,0,0,0,0,-5,-5,1,1,0,0,-2,-2,-2,-2};

 float vol9[64] = {1,0,1,0,1,0,1,0,0,0,1,0,1,0,1,0,0,0,1,0,1,0,1,0,0,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0,1,0,1,0,1,0,0,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0};
 float mel9[64] = {0,0,8,0,7,0,5,0,0,0,8,0,7,0,5,0,0,0,8,0,7,0,5,0,0,0,7,0,5,0,3,0,0,0,8,0,7,0,5,0,0,0,8,0,7,0,5,0,0,0,8,0,7,0,5,0,10,0,8,0,7,0,3,0};
 float bas9[64] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,-2,-2,-2,-2,-2,-2,-2,-2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,3,3,3,3,-2,-2,-2,-2};

 float vol10[64] = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1};
 float mel10[64] = {0,0,0,0,8,7,5,0,0,0,0,8,7,5,0,0,0,0,8,7,5,0,0,0,0,8,7,5,-2,-2,-2,-2,8,5,1,-2,-2,-2,-2,8,5,1,-2,-2,-2,-2,8,5,1,0,1,5,8,12,13,8,5};
 float bas10[64] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-2,-2,-2,-2,1,1,1,1,-2,-2,-2,-2,1,1,1,1,-2,-2,-2,-2,1,1,1,1,0,0,0,0,-2,-2,-2,-2};

 float vol11[64] = {1,0,0,1, 0,0,1,0, 0,1,0,0,  1,0,0,1, 1,0,1,0, 0,1,0,0, 1,0,0,1, 0,0,1,1, 1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,0,1, 0,0,1,0 };
 float mel11[64] = {7,0,0,0, 0,0,7,0, 0,2,0,0,  7,0,0,3, 0,0,7,0, 0,5,0,0, 7,0,0,3, 0,0,7,8, 7,0,0,0, 0,0,7,0, 0,2,0,0, 7,0,0,3, 0,0,7,0, 0,5,0,0, 3,0,0,2, 0,0,1,0 };
 float bas11[64] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,3,2,1,0};

 float vol12[64] = {0,1,1,1,    0,0,1,0,     0,0,1,0,    0,0,1,0,    0,0,1,0,    0,0,1,0,    0,0,1,1,   0,0,1,1,   0,1,1,1,     0,0,1,0,    0,0,1,0,    0,0,1,0,    0,0,1,0,    0,0,1,0,    0,0,1,1,    0,0,1,1 };
 float mel12[64] = {0,0,-5,-12, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,3, 0,0,0,2, 0,0,-5,-12, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,-2, 0,0,0,-1};
 float bas12[64] = {0,0,0,0,1,1,1,1,2,2,2,2,0,0,0,0,1,1,1,1,2,2,2,2,0,0,0,0,1,1,1,1,2,2,2,2,0,0,0,0,1,1,1,1,2,2,2,2,1,1,1,1,2,2,2,2,0,0,0,0,-1,-1,-1,-1};

 float vol13[64] = {1,1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1,0};
 float bas13[64] = {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,-5,-5,-5,-5,-5,-5,-5,-5,-4,-4,-4,-4,-4,-4,-4,-4,-6,-6,-6,-6,-6,-6,-3,-3,-3,-3,-3,-3,-7,-7,-7,-7};
 float mel13[64] = {0,12,0,1,8,0,7,0,0,12,0,1,8,0,7,0,0,12,0,8,1,0,7,0,0,12,0,1,8,0,7,0,0,12,0,1,8,0,7,0,0,12,0,1,8,0,7,0,0,12,0,8,1,0,7,0,0,12,0,1,8,0,7,0};

 float vol14[64] = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1};
 float mel14[64] = {12,0,-5,-12,12,12,0,12,12,12,0,12,12,12,0,12,12,12,0,12,12,12,0,12,12,12,0,3,12,12,0,2,12,0,-5,-12,12,0,12,12,12,0,12,12,12,0,12,12,12,0,12,12,12,0,12,12,12,0,-2,12,12,0,-1};
 float bas14[64] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,5,5,5,5,5,5,5,5,3,3,3,3,3,3,3,3};

 float vol15[64] = { 1,1,1,1, 0,0,1,0,  1,1,1,1, 0,0,1,0,  1,1,1,1, 0,0,1,0,  1,1,1,1, 0,0,1,0,  1,1,1,1, 0,0,1,0,  1,1,1,1, 0,0,1,0,  1,1,1,1, 0,0,1,0,  1,1,1,1, 0,0,1,0};
 float mel15[64] = {0,-5,-12,3,0,0,2,0,0,-5,-12,3,0,0,2,0,0,-5,-12,3,0,0,2,0,0,-5,-12,3,0,0,2,0,-2,-5,-9,3,0,0,2,0,-2,-5,-9,3,0,0,2,0,-1,-5,-10,5,0,0,2,0,-1,-5,-10,5,0,0,7,0};
 float bas15[65] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,2,2,2,2,7,7};

 float vol16[64] = { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,    1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1};
 float bas16[64] = {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,3,3,3,3,3,3,3,3,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,3,3,3,3,3,3,3,3,1,1,1,1,1,1,1,1};
 float mel16[64] = {0,0,0,-4,-5,0,0,0,0,0,0,0,-4,-5,-5,-4,0,0,0,-4,-5,0,0,0,0,0,0,0,-4,-5,-5,-4,0,0,0,-4,-5,0,0,0,0,0,0,0,-4,-5,-5,-4,0,0,0,-4,-5,0,0,0,0,0,0,0,-4,-5,-5,-4};

#define NUM_TRACKS 8
 float *volume_a[NUM_TRACKS] = { vol1, vol2, vol3, vol4, vol5, vol6, vol7, vol8 };
 float *volume_b[NUM_TRACKS] = { vol1, vol10, vol11, vol12, vol13, vol14, vol15, vol16 }; 
 float *bass_a[NUM_TRACKS] = { bas1, bas2, bas3, bas4, bas5, bas6, bas7, bas8 };
 float *bass_b[NUM_TRACKS] = { bas1, bas10, bas11, bas12, bas13, bas14, bas15, bas16 }; 
 float *melody_a[NUM_TRACKS] = { mel1, mel2, mel3, mel4, mel5, mel6, mel7, mel8 };
 float *melody_b[NUM_TRACKS] = { mel1, mel10, mel11, mel12, mel13, mel14, mel15, mel16 }; 

float **volume_x[2] = { &volume_a[0], &volume_b[0] };
float **bass_x[2] =  { &bass_a[0], &bass_b[0] };
float **melody_x[2] =  { &melody_a[0], &melody_b[0] };

#define NUM_MELODIES 16
float *melodies[NUM_MELODIES] = { mel1, mel2, mel3, mel4, mel5, mel6, mel7, mel8, mel9, mel10, mel11, mel12, mel13, mel14, mel15, mel16 };
float *basses[NUM_MELODIES] = { bas1, bas2, bas3, bas4, bas5, bas6, bas7, bas8, bas9, bas10, bas11, bas12, bas13, bas14, bas15, bas16 };
float *volumes[NUM_MELODIES] = { vol1, vol2, vol3, vol4, vol5, vol6, vol7, vol8, vol9, vol10, vol11, vol12, vol13, vol14, vol15, vol16 };

/*
int main() {

	for (int i = 0; i < NUM_TRACKS; i++) {
		for (int x = 0; x < 64; x++) {
			printf("%i,", mel[i][x]);
		}
		printf("\n");
	}

	return 0;
}
*/
