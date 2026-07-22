#include <gst/gst.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    printf("Wild Animal Detection initialized successfully.\n");

    return 0;
}