#include "Application.h"


int main(const int argc, char **argv) {
    const Application application(argc, argv);
    application.start();
    return 0;
}
