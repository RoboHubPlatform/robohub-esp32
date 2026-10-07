/*
 * app_main.c — Where ESP-IDF's entry name meets a program's.
 *
 * ESP-IDF starts an application at `app_main`, called from its own startup
 * after the scheduler is running; a Rho program's entry is `main`. This is the
 * one place the two names meet.
 *
 * It is a source of the program's own component rather than of the platform
 * library, because that is where the linker puts the program's `main`: a bridge
 * compiled into a library beside it is a reference to a symbol the linker has
 * already been past, and reports as one that does not exist.
 *
 * @project RoboHub Platform
 * @license MIT
 */

extern int main(void);

void app_main(void)
{
    (void)main();
}
