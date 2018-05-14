SkQP
====

Development APKs of SkQP are kept in Google storage.  Each file in named
with a abbreviated Git hash that points at the commit in the Skia repository it
was built with.

These are universal APKs that contain native libraries for armeabi-v7a,
arm64-v8a, x86, and x86\_64 architectures. The most recent is listed first:

<!--
#!/bin/sh
BRANCH=origin/skqp/dev
for commit in $(git log $BRANCH -30 -\-format=%H) ; do
    U='https://storage.googleapis.com/skia-skqp/skqp-universal-'
    URL="${U}$(git log -1 -\-format=%h $commit).apk"
    if [ 200 -ne "$(curl -s -o /dev/null -w "%{http_code}" "$URL")" ] ; then
        continue
    fi
    TZ='' git log \
        -\-date='format-local:%Y-%m-%d %H:%M:%S %Z' -1 $commit \
        -\-format="  * [\`%h\`](${U}%h.apk)%n    | \`%cd\` | %<(50,trunc)%s"
done
-->

  * [`120ffdd88c`](https://storage.googleapis.com/skia-skqp/skqp-universal-120ffdd88c.apk)
    | `2018-02-23 19:08:26 UTC` | SkQP:  do test filtering correctly
  * [`5eff3287c9`](https://storage.googleapis.com/skia-skqp/skqp-universal-5eff3287c9.apk)
    | `2018-02-22 14:00:28 UTC` | Remove branch lookup from skqp-dev
  * [`103b402a29`](https://storage.googleapis.com/skia-skqp/skqp-universal-103b402a29.apk)
    | `2018-02-21 20:56:10 UTC` | SkQP: run a single test
  * [`38efb0d355`](https://storage.googleapis.com/skia-skqp/skqp-universal-38efb0d355.apk)
    | `2018-02-20 20:48:45 UTC` | Add SKQP bot to build universal APK to master
  * [`d69db48840`](https://storage.googleapis.com/skia-skqp/skqp-universal-d69db48840.apk)
    | `2018-02-13 21:07:09 UTC` | SkQP: relax five tests
  * [`337919990b`](https://storage.googleapis.com/skia-skqp/skqp-universal-337919990b.apk)
    | `2018-02-13 19:33:12 UTC` | SkQP:  debug option, and fix a bug

To run tests:

    adb install -r skqp-universal-{APK_SHA_HERE}.apk
    adb logcat -c
    adb shell am instrument -w org.skia.skqp

Monitor the output with:

    adb logcat org.skia.skqp skia "*:S"

Note the test's output path on the device.  It will look something like this:

    01-23 15:22:12.688 27158 27173 I org.skia.skqp:
    output written to "/storage/emulated/0/Android/data/org.skia.skqp/files/output"

Retrieve and view the report with:

    OUTPUT_LOCATION="/storage/emulated/0/Android/data/org.skia.skqp/files/output"
    adb pull $OUTPUT_LOCATION /tmp/

Open the file `/tmp/output/skqp_report/report.html` .

* * *

For more information about building your own APK, refer to
https://skia.googlesource.com/skia/+/master/tools/skqp/README.md
