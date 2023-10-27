#include <stdio.h>
#include <math.h>

int main() {
    // Define the ratios for the A natural minor scale.
    float scaleRatios[7] = {
        1.0,     // A
        1.1225,  // B
        1.1892,  // C
        1.3348,  // D
        1.4983,  // E
        1.5874,  // F
        1.7818   // G
    };

    // Print the array declaration
    printf("float minorScaleNoteMultipliers[63] = {\n");

    // For each octave from -1 to 8
    for (int octave = -1; octave <= 10; octave++) {
        for (int i = 0; i < 7; i++) {
            // Calculate the multiplier for this note in this octave
            float multiplier = scaleRatios[i] * powf(2.0, octave);
            printf("    %.4f", multiplier);

            // If it's not the last element, print a comma
            if (octave != 8 || i != 6) {
                printf(",");
            }

            // If it's the last element of a line, print a newline
            if ((i + 1) % 7 == 0) {
                printf("\n");
            } else {
                printf(" ");
            }
        }
    }

    printf("};\n");

    return 0;
}
