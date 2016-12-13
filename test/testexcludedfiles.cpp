/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>

#include "excludedfiles.h"
#include "discoveryphase.h"

// Include csync exclude tests to run them again with the libsync hook
#include "csync.h"
#include "csync_private.h"
#define LIBSYNC_TEST
OCC::ExcludeHookData* excludeHookDataPtr;

// Regex enabled
#define HOOK &OCC::excluded_traversal_hook, excludeHookDataPtr

// Regex disabled
//#define HOOK 0, 0

#include "../csync/tests/csync_tests/check_csync_exclude.c"

using namespace OCC;

#define STR_(X) #X
#define STR(X) STR_(X)
#define BIN_PATH STR(OWNCLOUD_BIN_PATH)

class TestExcludedFiles: public QObject
{
    Q_OBJECT

private slots:
    void testFun()
    {
        auto & excluded = ExcludedFiles::instance();
        bool excludeHidden = true;
        bool keepHidden = false;

        QVERIFY(!excluded.isExcluded("/a/b", "/a", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/b~", "/a", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/.b", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.b", "/a", excludeHidden));

        QString path(BIN_PATH);
        path.append("/sync-exclude.lst");
        excluded.addExcludeFilePath(path);
        excluded.reloadExcludes();

        QVERIFY(!excluded.isExcluded("/a/b", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/b~", "/a", keepHidden));
        QVERIFY(!excluded.isExcluded("/a/.b", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.Trashes", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/foo_conflict-bar", "/a", keepHidden));
        QVERIFY(excluded.isExcluded("/a/.b", "/a", excludeHidden));
    }

//    void csync_test()
//    {
//        void* state = 0;
//        setup(&state);

//        OCC::ExcludeHookData excludeHookData;
//        excludeHookData.excludes = &((CSYNC*)state)->excludes;
//        excludeHookDataPtr = &excludeHookData;

//        check_csync_excluded_traversal(&state);
//        teardown(&state);
//    }

    void csync_perf_test()
    {
        void* state = 0;
        setup_init(&state);

        OCC::ExcludeHookData excludeHookData;
        excludeHookData.excludes = &((CSYNC*)state)->excludes;
        excludeHookDataPtr = &excludeHookData;

        check_csync_excluded_performance(&state);
        teardown(&state);
    }
};

QTEST_APPLESS_MAIN(TestExcludedFiles)
#include "testexcludedfiles.moc"
