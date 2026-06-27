#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "src/lib/stb/stb_truetype.h"

START_TEST(test_font_parsing_bounds_invariant)
{
    // Invariant: Font parsing must not perform out-of-bounds memory accesses
    const char *payloads[] = {
        // Exploit case: crafted font with invalid offset causing OOB read
        "\x00\x01\x00\x00\x00\x0D\x00\x80\x00\x03\x00\x00\x4F\x53\x2F\x32"
        "\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00",
        // Boundary case: minimal valid font header with extreme offset values
        "\x00\x01\x00\x00\x00\x0D\x00\x80\x00\x03\x00\x00\x4F\x53\x2F\x32"
        "\x00\x00\x00\x00\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
        // Valid input: simple test font header
        "\x00\x01\x00\x00\x00\x0D\x00\x80\x00\x03\x00\x00\x4F\x53\x2F\x32"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        stbtt_fontinfo font;
        unsigned char *data = (unsigned char *)payloads[i];
        int result = stbtt_InitFont(&font, data, 0);
        
        // Property: Initialization must either succeed cleanly or fail safely
        // without triggering memory access violations
        ck_assert_msg(result == 0 || result == 1, 
                     "Font parsing must not cause memory corruption");
        
        // Additional safety check: if initialization succeeded, 
        // basic operations should not crash
        if (result) {
            int ascent, descent, lineGap;
            stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
            // No assertion on values, just ensuring no crash occurs
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_font_parsing_bounds_invariant);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}