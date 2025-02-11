#include <stdio.h>
#include "pico/stdlib.h"

int main()
{
    gpio_init(0);
    gpio_set_dir(0, GPIO_OUT);

    while(1) {
        gpio_put(0, 1);
        sleep_ms(1000);
        gpio_put(0,0);
        sleep_ms(1000);
    }
}