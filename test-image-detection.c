// Simple test program for image detection logic
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

static const char *image_extensions[] = {
    ".jpg", ".jpeg", ".png", ".webp", ".gif", ".jxl", NULL
};

static bool is_image_file(const char *path) {
    if (!path) return false;
    const char *ext = strrchr(path, '.');
    if (!ext) return false;

    char ext_lower[16] = {0};
    for (int i = 0; ext[i] && i < 15; i++) {
        ext_lower[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
    }

    for (int i = 0; image_extensions[i]; i++) {
        if (strcmp(ext_lower, image_extensions[i]) == 0) {
            return true;
        }
    }
    return false;
}

int main() {
    const char *test_files[] = {
        "/path/to/image.png",
        "/path/to/image.PNG",
        "/path/to/image.jpg",
        "/path/to/image.JPEG",
        "/path/to/image.webp",
        "/path/to/image.gif",
        "/path/to/image.jxl",
        "/path/to/video.mp4",
        "/path/to/video.mkv",
        "/path/to/file.txt",
        NULL
    };

    printf("Image Detection Tests:\n");
    printf("=====================\n\n");

    for (int i = 0; test_files[i]; i++) {
        bool is_img = is_image_file(test_files[i]);
        printf("%-30s -> %s\n", test_files[i], is_img ? "IMAGE ✓" : "NOT IMAGE");
    }

    return 0;
}
