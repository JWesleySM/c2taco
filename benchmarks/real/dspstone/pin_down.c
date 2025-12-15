#define STORAGE_CLASS register
#define TYPE  int

void pin_down(TYPE * px, int LENGTH)
{
	STORAGE_CLASS TYPE    i;

	for (i = 0; i < LENGTH; ++i) {
		*px++ = 1;
	}

}
