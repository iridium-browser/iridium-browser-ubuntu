// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.ref.WeakReference;
import java.util.concurrent.CountDownLatch;

/**
 * Part of the test suite for the Java Bridge. Tests a number of features including ...
 * - The type of injected objects
 * - The type of their methods
 * - Replacing objects
 * - Removing objects
 * - Access control
 * - Calling methods on returned objects
 * - Multiply injected objects
 * - Threading
 * - Inheritance
 */
@SuppressFBWarnings("URF_UNREAD_PUBLIC_OR_PROTECTED_FIELD")
public class JavaBridgeBasicsTest extends JavaBridgeTestBase {
    @SuppressFBWarnings("CHROMIUM_SYNCHRONIZED_METHOD")
    private class TestController extends Controller {
        private int mIntValue;
        private long mLongValue;
        private String mStringValue;
        private boolean mBooleanValue;

        public synchronized void setIntValue(int x) {
            mIntValue = x;
            notifyResultIsReady();
        }
        public synchronized void setLongValue(long x) {
            mLongValue = x;
            notifyResultIsReady();
        }
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }
        public synchronized void setBooleanValue(boolean x) {
            mBooleanValue = x;
            notifyResultIsReady();
        }

        public synchronized int waitForIntValue() {
            waitForResult();
            return mIntValue;
        }
        public synchronized long waitForLongValue() {
            waitForResult();
            return mLongValue;
        }
        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }
        public synchronized boolean waitForBooleanValue() {
            waitForResult();
            return mBooleanValue;
        }

        public synchronized String getStringValue() {
            return mStringValue;
        }
    }

    private static class ObjectWithStaticMethod {
        public static String staticMethod() {
            return "foo";
        }
    }

    TestController mTestController;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestController = new TestController();
        injectObjectAndReload(mTestController, "testController");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    protected String executeJavaScriptAndGetStringResult(String script) throws Throwable {
        executeJavaScript("testController.setStringValue(" + script + ");");
        return mTestController.waitForStringValue();
    }

    // Note that this requires that we can pass a JavaScript boolean to Java.
    private void executeAndSetIfException(String script) throws Throwable {
        executeJavaScript("try {"
                + script + ";"
                + "  testController.setBooleanValue(false);"
                + "} catch (exception) {"
                + "  testController.setBooleanValue(true);"
                + "}");
    }

    private void assertRaisesException(String script) throws Throwable {
        executeAndSetIfException(script);
        assertTrue(mTestController.waitForBooleanValue());
    }

    private void assertNoRaisedException(String script) throws Throwable {
        executeAndSetIfException(script);
        assertFalse(mTestController.waitForBooleanValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTypeOfInjectedObject() throws Throwable {
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAdditionNotReflectedUntilReload() throws Throwable {
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().addPossiblyUnsafeJavascriptInterface(
                        new Object(), "testObject", null);
            }
        });
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
        synchronousPageReload();
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testRemovalNotReflectedUntilReload() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() {
                mTestController.setStringValue("I'm here");
            }
        }, "testObject");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        executeJavaScript("testObject.method()");
        assertEquals("I'm here", mTestController.waitForStringValue());
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().removeJavascriptInterface("testObject");
            }
        });
        // Check that the Java object is being held by the Java bridge, thus it's not
        // collected. Note that despite that what JavaDoc says about invoking "gc()", both Dalvik
        // and ART actually run the collector if called via Runtime.
        Runtime.getRuntime().gc();
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        executeJavaScript("testObject.method()");
        assertEquals("I'm here", mTestController.waitForStringValue());
        synchronousPageReload();
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testRemoveObjectNotAdded() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().removeJavascriptInterface("foo");
                getContentViewCore().getWebContents().getNavigationController().reload(true);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof foo"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTypeOfMethod() throws Throwable {
        assertEquals("function",
                executeJavaScriptAndGetStringResult("typeof testController.setStringValue"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTypeOfInvalidMethod() throws Throwable {
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testController.foo"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallingInvalidMethodRaisesException() throws Throwable {
        assertRaisesException("testController.foo()");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testUncaughtJavaExceptionRaisesJavaScriptException() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() {
                throw new RuntimeException("foo");
            }
        }, "testObject");
        assertRaisesException("testObject.method()");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallingAsConstructorRaisesException() throws Throwable {
        assertRaisesException("new testController.setStringValue('foo')");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallingOnNonInjectedObjectRaisesException() throws Throwable {
        assertRaisesException("testController.setStringValue.call({}, 'foo')");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallingOnInstanceOfOtherClassRaisesException() throws Throwable {
        injectObjectAndReload(new Object(), "testObject");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
        assertEquals("function",
                executeJavaScriptAndGetStringResult("typeof testController.setStringValue"));
        assertRaisesException("testController.setStringValue.call(testObject, 'foo')");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTypeOfStaticMethod() throws Throwable {
        injectObjectAndReload(new ObjectWithStaticMethod(), "testObject");
        executeJavaScript("testController.setStringValue(typeof testObject.staticMethod)");
        assertEquals("function", mTestController.waitForStringValue());
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallStaticMethod() throws Throwable {
        injectObjectAndReload(new ObjectWithStaticMethod(), "testObject");
        executeJavaScript("testController.setStringValue(testObject.staticMethod())");
        assertEquals("foo", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPrivateMethodNotExposed() throws Throwable {
        injectObjectAndReload(new Object() {
            private void method() {}
            protected void method2() {}
        }, "testObject");
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.method"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.method2"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReplaceInjectedObject() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() {
                mTestController.setStringValue("object 1");
            }
        }, "testObject");
        executeJavaScript("testObject.method()");
        assertEquals("object 1", mTestController.waitForStringValue());

        injectObjectAndReload(new Object() {
            public void method() {
                mTestController.setStringValue("object 2");
            }
        }, "testObject");
        executeJavaScript("testObject.method()");
        assertEquals("object 2", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testInjectNullObjectIsIgnored() throws Throwable {
        injectObjectAndReload(null, "testObject");
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReplaceInjectedObjectWithNullObjectIsIgnored() throws Throwable {
        injectObjectAndReload(new Object(), "testObject");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        injectObjectAndReload(null, "testObject");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallOverloadedMethodWithDifferentNumberOfArguments() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() {
                mTestController.setStringValue("0 args");
            }

            public void method(int x) {
                mTestController.setStringValue("1 arg");
            }

            public void method(int x, int y) {
                mTestController.setStringValue("2 args");
            }
        }, "testObject");
        executeJavaScript("testObject.method()");
        assertEquals("0 args", mTestController.waitForStringValue());
        executeJavaScript("testObject.method(42)");
        assertEquals("1 arg", mTestController.waitForStringValue());
        executeJavaScript("testObject.method(null)");
        assertEquals("1 arg", mTestController.waitForStringValue());
        executeJavaScript("testObject.method(undefined)");
        assertEquals("1 arg", mTestController.waitForStringValue());
        executeJavaScript("testObject.method(42, 42)");
        assertEquals("2 args", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallMethodWithWrongNumberOfArgumentsRaisesException() throws Throwable {
        assertRaisesException("testController.setIntValue()");
        assertRaisesException("testController.setIntValue(42, 42)");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testObjectPersistsAcrossPageLoads() throws Throwable {
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
        synchronousPageReload();
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCustomPropertiesCleanedUpOnPageReloads() throws Throwable {
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
        executeJavaScript("testController.myProperty = 42;");
        assertEquals("42", executeJavaScriptAndGetStringResult("testController.myProperty"));
        synchronousPageReload();
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
        assertEquals("undefined", executeJavaScriptAndGetStringResult("testController.myProperty"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testSameObjectInjectedMultipleTimes() throws Throwable {
        class TestObject {
            private int mNumMethodInvocations;

            public void method() {
                mTestController.setIntValue(++mNumMethodInvocations);
            }
        }
        final TestObject testObject = new TestObject();
        injectObjectsAndReload(testObject, "testObject1", testObject, "testObject2", null);
        executeJavaScript("testObject1.method()");
        assertEquals(1, mTestController.waitForIntValue());
        executeJavaScript("testObject2.method()");
        assertEquals(2, mTestController.waitForIntValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallMethodOnReturnedObject() throws Throwable {
        injectObjectAndReload(new Object() {
            public Object getInnerObject() {
                return new Object() {
                    public void method(int x) {
                        mTestController.setIntValue(x);
                    }
                };
            }
        }, "testObject");
        executeJavaScript("testObject.getInnerObject().method(42)");
        assertEquals(42, mTestController.waitForIntValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReturnedObjectInjectedElsewhere() throws Throwable {
        class InnerObject {
            private int mNumMethodInvocations;

            public void method() {
                mTestController.setIntValue(++mNumMethodInvocations);
            }
        }
        final InnerObject innerObject = new InnerObject();
        final Object object = new Object() {
            public InnerObject getInnerObject() {
                return innerObject;
            }
        };
        injectObjectsAndReload(object, "testObject", innerObject, "innerObject", null);
        executeJavaScript("testObject.getInnerObject().method()");
        assertEquals(1, mTestController.waitForIntValue());
        executeJavaScript("innerObject.method()");
        assertEquals(2, mTestController.waitForIntValue());
    }

    // Verify that Java objects returned from bridge object methods are dereferenced
    // on the Java side once they have been fully dereferenced on the JS side.
    // Failing this test would mean that methods returning objects effectively create a memory
    // leak.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    @CommandLineFlags.Add("js-flags=--expose-gc")
    public void testReturnedObjectIsGarbageCollected() throws Throwable {
        // Make sure V8 exposes "gc" property on the global object (enabled with --expose-gc flag)
        assertEquals("function", executeJavaScriptAndGetStringResult("typeof gc"));
        class InnerObject {
        }
        class TestObject {
            public InnerObject getInnerObject() {
                InnerObject inner = new InnerObject();
                mWeakRefForInner = new WeakReference<InnerObject>(inner);
                return inner;
            }
            // A weak reference is used to check InnerObject instance reachability.
            WeakReference<InnerObject> mWeakRefForInner;
        }
        TestObject object = new TestObject();
        injectObjectAndReload(object, "testObject");
        // Initially, store a reference to the inner object in JS to make sure it's not
        // garbage-collected prematurely.
        assertEquals("object", executeJavaScriptAndGetStringResult(
                        "(function() { "
                        + "globalInner = testObject.getInnerObject(); return typeof globalInner; "
                        + "})()"));
        assertTrue(object.mWeakRefForInner.get() != null);
        // Check that returned Java object is being held by the Java bridge, thus it's not
        // collected.  Note that despite that what JavaDoc says about invoking "gc()", both Dalvik
        // and ART actually run the collector.
        Runtime.getRuntime().gc();
        assertTrue(object.mWeakRefForInner.get() != null);
        // Now dereference the inner object in JS and run GC to collect the interface object.
        assertEquals("true", executeJavaScriptAndGetStringResult(
                        "(function() { "
                        + "delete globalInner; gc(); return (typeof globalInner == 'undefined'); "
                        + "})()"));
        // Force GC on the Java side again. The bridge had to release the inner object, so it must
        // be collected this time.
        Runtime.getRuntime().gc();
        assertEquals(null, object.mWeakRefForInner.get());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testSameReturnedObjectUsesSameWrapper() throws Throwable {
        class InnerObject {
        }
        final InnerObject innerObject = new InnerObject();
        final Object injectedTestObject = new Object() {
            public InnerObject getInnerObject() {
                return innerObject;
            }
        };
        injectObjectAndReload(injectedTestObject, "injectedTestObject");
        executeJavaScript("inner1 = injectedTestObject.getInnerObject()");
        executeJavaScript("inner2 = injectedTestObject.getInnerObject()");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof inner1"));
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof inner2"));
        assertEquals("true", executeJavaScriptAndGetStringResult("inner1 === inner2"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMethodInvokedOnBackgroundThread() throws Throwable {
        injectObjectAndReload(new Object() {
            public void captureThreadId() {
                mTestController.setLongValue(Thread.currentThread().getId());
            }
        }, "testObject");
        executeJavaScript("testObject.captureThreadId()");
        final long threadId = mTestController.waitForLongValue();
        assertFalse(threadId == Thread.currentThread().getId());
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertFalse(threadId == Thread.currentThread().getId());
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testBlockingUiThreadDoesNotBlockCallsFromJs() throws Throwable {
        class TestObject {
            private CountDownLatch mLatch;
            public TestObject() {
                mLatch = new CountDownLatch(1);
            }
            public boolean waitOnTheLatch() throws Exception {
                return mLatch.await(scaleTimeout(10000),
                        java.util.concurrent.TimeUnit.MILLISECONDS);
            }
            public void unlockTheLatch() throws Exception {
                mTestController.setStringValue("unlocked");
                mLatch.countDown();
            }
        }
        final TestObject testObject = new TestObject();
        injectObjectAndReload(testObject, "testObject");
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                // loadUrl is asynchronous, the JS code will start running on the renderer
                // thread. As soon as we exit loadUrl, the browser UI thread will be stuck waiting
                // on the latch. If blocking the browser thread blocks Java Bridge, then the call
                // to "unlockTheLatch()" will be executed after the waiting timeout, thus the
                // string value will not yet be updated by the injected object.
                mTestController.setStringValue("locked");
                getWebContents().getNavigationController().loadUrl(new LoadUrlParams(
                        "javascript:(function() { testObject.unlockTheLatch() })()"));
                try {
                    assertTrue(testObject.waitOnTheLatch());
                } catch (Exception e) {
                    android.util.Log.e("JavaBridgeBasicsTest", "Wait exception", e);
                    Assert.fail("Wait exception");
                }
                assertEquals("unlocked", mTestController.getStringValue());
            }
        });
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPublicInheritedMethod() throws Throwable {
        class Base {
            public void method(int x) {
                mTestController.setIntValue(x);
            }
        }
        class Derived extends Base {
        }
        injectObjectAndReload(new Derived(), "testObject");
        assertEquals("function", executeJavaScriptAndGetStringResult("typeof testObject.method"));
        executeJavaScript("testObject.method(42)");
        assertEquals(42, mTestController.waitForIntValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPrivateInheritedMethod() throws Throwable {
        class Base {
            private void method() {}
        }
        class Derived extends Base {
        }
        injectObjectAndReload(new Derived(), "testObject");
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject.method"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testOverriddenMethod() throws Throwable {
        class Base {
            public void method() {
                mTestController.setStringValue("base");
            }
        }
        class Derived extends Base {
            @Override
            public void method() {
                mTestController.setStringValue("derived");
            }
        }
        injectObjectAndReload(new Derived(), "testObject");
        executeJavaScript("testObject.method()");
        assertEquals("derived", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testEnumerateMembers() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() {}
            private void privateMethod() {}
            @SuppressFBWarnings("UUF_UNUSED")
            public int field;
            @SuppressFBWarnings("UUF_UNUSED")
            private int mPrivateField;
        }, "testObject");
        executeJavaScript(
                "var result = \"\"; "
                + "for (x in testObject) { result += \" \" + x } "
                + "testController.setStringValue(result);");
        assertEquals(" equals getClass hashCode method notify notifyAll toString wait",
                mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReflectPublicMethod() throws Throwable {
        injectObjectAndReload(new Object() {
            public Class<?> myGetClass() {
                return getClass();
            }

            public String method() {
                return "foo";
            }
        }, "testObject");
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "testObject.myGetClass().getMethod('method', null).invoke(testObject, null)"
                + ".toString()"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReflectPublicField() throws Throwable {
        injectObjectAndReload(new Object() {
            public Class<?> myGetClass() {
                return getClass();
            }

            public String field = "foo";
        }, "testObject");
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "testObject.myGetClass().getField('field').get(testObject).toString()"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReflectPrivateMethodRaisesException() throws Throwable {
        injectObjectAndReload(new Object() {
            public Class<?> myGetClass() {
                return getClass();
            }

            private void method() {};
        }, "testObject");
        assertRaisesException("testObject.myGetClass().getMethod('method', null)");
        // getDeclaredMethod() is able to access a private method, but invoke()
        // throws a Java exception.
        assertRaisesException(
                "testObject.myGetClass().getDeclaredMethod('method', null)."
                + "invoke(testObject, null)");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReflectPrivateFieldRaisesException() throws Throwable {
        injectObjectAndReload(new Object() {
            public Class<?> myGetClass() {
                return getClass();
            }

            @SuppressFBWarnings("UUF_UNUSED")
            private int mField;
        }, "testObject");
        String fieldName = "mField";
        assertRaisesException("testObject.myGetClass().getField('" + fieldName + "')");
        // getDeclaredField() is able to access a private field, but getInt()
        // throws a Java exception.
        assertNoRaisedException("testObject.myGetClass().getDeclaredField('" + fieldName + "')");
        assertRaisesException(
                "testObject.myGetClass().getDeclaredField('" + fieldName + "').getInt(testObject)");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAllowNonAnnotatedMethods() throws Throwable {
        injectObjectAndReload(new Object() {
            public String allowed() {
                return "foo";
            }
        }, "testObject", null);

        // Test calling a method of an explicitly inherited class (Base#allowed()).
        assertEquals("foo", executeJavaScriptAndGetStringResult("testObject.allowed()"));

        // Test calling a method of an implicitly inherited class (Object#toString()).
        assertEquals("string", executeJavaScriptAndGetStringResult("typeof testObject.toString()"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAllowOnlyAnnotatedMethods() throws Throwable {
        injectObjectAndReload(new Object() {
            @JavascriptInterface
            public String allowed() {
                return "foo";
            }

            public String disallowed() {
                return "bar";
            }
        }, "testObject", JavascriptInterface.class);

        // getClass() is an Object method and does not have the @JavascriptInterface annotation and
        // should not be able to be called.
        assertRaisesException("testObject.getClass()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testObject.getClass"));

        // allowed() is marked with the @JavascriptInterface annotation and should be allowed to be
        // called.
        assertEquals("foo", executeJavaScriptAndGetStringResult("testObject.allowed()"));

        // disallowed() is not marked with the @JavascriptInterface annotation and should not be
        // able to be called.
        assertRaisesException("testObject.disallowed()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testObject.disallowed"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAnnotationRequirementRetainsPropertyAcrossObjects() throws Throwable {
        class Test {
            @JavascriptInterface
            public String safe() {
                return "foo";
            }

            public String unsafe() {
                return "bar";
            }
        }

        class TestReturner {
            @JavascriptInterface
            public Test getTest() {
                return new Test();
            }
        }

        // First test with safe mode off.
        injectObjectAndReload(new TestReturner(), "unsafeTestObject", null);

        // safe() should be able to be called regardless of whether or not we are in safe mode.
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "unsafeTestObject.getTest().safe()"));
        // unsafe() should be able to be called because we are not in safe mode.
        assertEquals("bar", executeJavaScriptAndGetStringResult(
                "unsafeTestObject.getTest().unsafe()"));

        // Now test with safe mode on.
        injectObjectAndReload(new TestReturner(), "safeTestObject", JavascriptInterface.class);

        // safe() should be able to be called regardless of whether or not we are in safe mode.
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "safeTestObject.getTest().safe()"));
        // unsafe() should not be able to be called because we are in safe mode.
        assertRaisesException("safeTestObject.getTest().unsafe()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof safeTestObject.getTest().unsafe"));
        // getClass() is an Object method and does not have the @JavascriptInterface annotation and
        // should not be able to be called.
        assertRaisesException("safeTestObject.getTest().getClass()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof safeTestObject.getTest().getClass"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAnnotationDoesNotGetInherited() throws Throwable {
        class Base {
            @JavascriptInterface
            public void base() { }
        }

        class Child extends Base {
            @Override
            public void base() { }
        }

        injectObjectAndReload(new Child(), "testObject", JavascriptInterface.class);

        // base() is inherited.  The inherited method does not have the @JavascriptInterface
        // annotation and should not be able to be called.
        assertRaisesException("testObject.base()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testObject.base"));
    }

    @SuppressWarnings("javadoc")
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD})
    @interface TestAnnotation {
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCustomAnnotationRestriction() throws Throwable {
        class Test {
            @TestAnnotation
            public String checkTestAnnotationFoo() {
                return "bar";
            }

            @JavascriptInterface
            public String checkJavascriptInterfaceFoo() {
                return "bar";
            }
        }

        // Inject javascriptInterfaceObj and require the JavascriptInterface annotation.
        injectObjectAndReload(new Test(), "javascriptInterfaceObj", JavascriptInterface.class);

        // Test#testAnnotationFoo() should fail, as it isn't annotated with JavascriptInterface.
        assertRaisesException("javascriptInterfaceObj.checkTestAnnotationFoo()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof javascriptInterfaceObj.checkTestAnnotationFoo"));

        // Test#javascriptInterfaceFoo() should pass, as it is annotated with JavascriptInterface.
        assertEquals("bar", executeJavaScriptAndGetStringResult(
                "javascriptInterfaceObj.checkJavascriptInterfaceFoo()"));

        // Inject testAnnotationObj and require the TestAnnotation annotation.
        injectObjectAndReload(new Test(), "testAnnotationObj", TestAnnotation.class);

        // Test#testAnnotationFoo() should pass, as it is annotated with TestAnnotation.
        assertEquals("bar", executeJavaScriptAndGetStringResult(
                "testAnnotationObj.checkTestAnnotationFoo()"));

        // Test#javascriptInterfaceFoo() should fail, as it isn't annotated with TestAnnotation.
        assertRaisesException("testAnnotationObj.checkJavascriptInterfaceFoo()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testAnnotationObj.checkJavascriptInterfaceFoo"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAddJavascriptInterfaceIsSafeByDefault() throws Throwable {
        class Test {
            public String blocked() {
                return "bar";
            }

            @JavascriptInterface
            public String allowed() {
                return "bar";
            }
        }

        // Manually inject the Test object, making sure to use the
        // ContentViewCore#addJavascriptInterface, not the possibly unsafe version.
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().addJavascriptInterface(new Test(),
                        "testObject");
                getContentViewCore().getWebContents().getNavigationController().reload(true);
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);

        // Test#allowed() should pass, as it is annotated with JavascriptInterface.
        assertEquals("bar", executeJavaScriptAndGetStringResult(
                "testObject.allowed()"));

        // Test#blocked() should fail, as it isn't annotated with JavascriptInterface.
        assertRaisesException("testObject.blocked()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testObject.blocked"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testObjectsInspection() throws Throwable {
        class Test {
            @JavascriptInterface
            public String m1() {
                return "foo";
            }

            @JavascriptInterface
            public String m2() {
                return "bar";
            }

            @JavascriptInterface
            public String m2(int x) {
                return "bar " + x;
            }
        }

        final String jsObjectKeysTestTemplate = "Object.keys(%s).toString()";
        final String jsForInTestTemplate =
                "(function(){"
                + "  var s=[]; for(var m in %s) s.push(m); return s.join(\",\")"
                + "})()";
        final String inspectableObjectName = "testObj1";
        final String nonInspectableObjectName = "testObj2";

        // Inspection is enabled by default.
        injectObjectAndReload(new Test(), inspectableObjectName, JavascriptInterface.class);

        assertEquals("m1,m2", executeJavaScriptAndGetStringResult(
                        String.format(jsObjectKeysTestTemplate, inspectableObjectName)));
        assertEquals("m1,m2", executeJavaScriptAndGetStringResult(
                        String.format(jsForInTestTemplate, inspectableObjectName)));

        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentViewCore().setAllowJavascriptInterfacesInspection(false);
            }
        });

        injectObjectAndReload(new Test(), nonInspectableObjectName, JavascriptInterface.class);

        assertEquals("", executeJavaScriptAndGetStringResult(
                        String.format(jsObjectKeysTestTemplate, nonInspectableObjectName)));
        assertEquals("", executeJavaScriptAndGetStringResult(
                        String.format(jsForInTestTemplate, nonInspectableObjectName)));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAccessToObjectGetClassIsBlocked() throws Throwable {
        injectObjectAndReload(new Object(), "testObject");
        assertEquals("function", executeJavaScriptAndGetStringResult("typeof testObject.getClass"));
        assertRaisesException("testObject.getClass()");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReplaceJavascriptInterface() throws Throwable {
        class Test {
            public Test(int value) {
                mValue = value;
            }
            @JavascriptInterface
            public int getValue() {
                return mValue;
            }
            private int mValue;
        }
        injectObjectAndReload(new Test(13), "testObject");
        assertEquals("13", executeJavaScriptAndGetStringResult("testObject.getValue()"));
        // The documentation doesn't specify, what happens if the embedder is trying
        // to inject a different object under the same name. The current implementation
        // simply replaces the old object with the new one.
        injectObjectAndReload(new Test(42), "testObject");
        assertEquals("42", executeJavaScriptAndGetStringResult("testObject.getValue()"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMethodCalledOnAnotherInstance() throws Throwable {
        class TestObject {
            private int mIndex;
            TestObject(int index) {
                mIndex = index;
            }
            public void method() {
                mTestController.setIntValue(mIndex);
            }
        }
        final TestObject testObject1 = new TestObject(1);
        final TestObject testObject2 = new TestObject(2);
        injectObjectsAndReload(testObject1, "testObject1", testObject2, "testObject2", null);
        executeJavaScript("testObject1.method()");
        assertEquals(1, mTestController.waitForIntValue());
        executeJavaScript("testObject2.method()");
        assertEquals(2, mTestController.waitForIntValue());
        executeJavaScript("testObject1.method.call(testObject2)");
        assertEquals(2, mTestController.waitForIntValue());
        executeJavaScript("testObject2.method.call(testObject1)");
        assertEquals(1, mTestController.waitForIntValue());
    }
}
