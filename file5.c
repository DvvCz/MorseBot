#define MORSE_OK 0
#define MORSE_UNRECOGNIZED 1
#define MORSE_MAX_WORD_LEN 50

// Have to implement these myself to keep binary size small to fit onto the robot.
int	t_strcmp(char* s1, char* s2) {
	int i = 0;
	while (s1[i] == s2[i] && s1[i] != '\0' && s2[i] != '\0') i++;
	return (s1[i] - s2[i]);
}

unsigned int t_strlen(char* s) {
    unsigned int count = 0;
    while (*s++ != '\0') count++;
    return count;
}

// Translate from international morse code to regular ascii text (english).
int translateMorse(char* str, char* out) {
    uint16_t pos = 0;
	// char str[] = "Hello world";

	// Word length
    int len = 0;
    char word[MORSE_MAX_WORD_LEN] = "";

	// Total length of morse buffer minus 1
	unsigned int total_len_m1 = t_strlen(str) - 1;
    for (unsigned int i = 0; i <= total_len_m1; i++) {
        char c = str[i];
        
        if ( c == ' ' || i == total_len_m1 ) {
            if (i == total_len_m1) {
                word[len] = c;
                len++;
            }

            word[len] = '\0';

	        #define STRING_TEST(letter, str) if ( t_strcmp(word, str) == 0) { out[pos] = letter; } else
            print("Got word %s\n", word);

			STRING_TEST('a', ".-")
	    	STRING_TEST('b', "-...")
	    	STRING_TEST('c', "-.-.")
	    	STRING_TEST('d', "-..")
	    	STRING_TEST('e', ".")
	    	STRING_TEST('f', "..-.")
	    	STRING_TEST('g', "--.")
	    	STRING_TEST('h', "....")
	    	STRING_TEST('i', "..")
	    	STRING_TEST('j', ".---")
	    	STRING_TEST('k', "-.-")
	    	STRING_TEST('l', ".-..")
	    	STRING_TEST('m', "--")
	    	STRING_TEST('n', "-.")
	    	STRING_TEST('o', "---")
	    	STRING_TEST('p', ".--.")
	    	STRING_TEST('q', "--.-")
	    	STRING_TEST('r', ".-.")
	    	STRING_TEST('s', "...")
	    	STRING_TEST('t', "-")
	    	STRING_TEST('u', "..-")
	    	STRING_TEST('v', "...-")
	    	STRING_TEST('w', ".--")
	    	STRING_TEST('x', "-..-")
	    	STRING_TEST('y', "-.--")
	    	STRING_TEST('z', "--..")

        	// Word delimiter
        	STRING_TEST(' ', "/")

	    	// Numbers
	    	STRING_TEST('1', ".----")
	    	STRING_TEST('2', "..---")
        	STRING_TEST('3', "...--")
	    	STRING_TEST('4', "....-")
	    	STRING_TEST('5', ".....")
	    	STRING_TEST('6', "-....")
	    	STRING_TEST('7', "--...")
	    	STRING_TEST('8', "---..")
	    	STRING_TEST('9', "----.")
	    	STRING_TEST('0', "-----")
        	{
				// Shouldn't be possible.
            	print("Unrecognized %s\n", word);
				return MORSE_UNRECOGNIZED;
        	}

            len = 0;
        } else {
            word[len] = c;
            len++;
        }
    }

    out[pos] = '\0';
    return MORSE_OK;
}
