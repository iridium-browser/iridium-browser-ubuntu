// There are tests for computeStatistics() located in LayoutTests/fast/harness/perftests

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

(function () {
    var logLines = null;
    var completedIterations = -1;
    var callsPerIteration = 1;
    var currentTest = null;
    var results = [];
    var jsHeapResults = [];
    var iterationCount = undefined;

    var PerfTestRunner = {};

    // To make the benchmark results predictable, we replace Math.random with a
    // 100% deterministic alternative.
    PerfTestRunner.randomSeed = PerfTestRunner.initialRandomSeed = 49734321;

    PerfTestRunner.resetRandomSeed = function() {
        PerfTestRunner.randomSeed = PerfTestRunner.initialRandomSeed
    }

    PerfTestRunner.random = Math.random = function() {
        // Robert Jenkins' 32 bit integer hash function.
        var randomSeed = PerfTestRunner.randomSeed;
        randomSeed = ((randomSeed + 0x7ed55d16) + (randomSeed << 12))  & 0xffffffff;
        randomSeed = ((randomSeed ^ 0xc761c23c) ^ (randomSeed >>> 19)) & 0xffffffff;
        randomSeed = ((randomSeed + 0x165667b1) + (randomSeed << 5))   & 0xffffffff;
        randomSeed = ((randomSeed + 0xd3a2646c) ^ (randomSeed << 9))   & 0xffffffff;
        randomSeed = ((randomSeed + 0xfd7046c5) + (randomSeed << 3))   & 0xffffffff;
        randomSeed = ((randomSeed ^ 0xb55a4f09) ^ (randomSeed >>> 16)) & 0xffffffff;
        PerfTestRunner.randomSeed = randomSeed;
        return (randomSeed & 0xfffffff) / 0x10000000;
    };

    PerfTestRunner.now = window.performance && window.performance.now ? function () { return window.performance.now(); } : Date.now;

    PerfTestRunner.logInfo = function (text) {
        if (!window.testRunner)
            this.log(text);
    }

    PerfTestRunner.loadFile = function (path) {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", path, false);
        xhr.send(null);
        return xhr.responseText;
    }

    PerfTestRunner.computeStatistics = function (times, unit) {
        var data = times.slice();

        // Add values from the smallest to the largest to avoid the loss of significance
        data.sort(function(a,b){return a-b;});

        var middle = Math.floor(data.length / 2);
        var result = {
            min: data[0],
            max: data[data.length - 1],
            median: data.length % 2 ? data[middle] : (data[middle - 1] + data[middle]) / 2,
        };

        // Compute the mean and variance using Knuth's online algorithm (has good numerical stability).
        var squareSum = 0;
        result.values = times;
        result.mean = 0;
        for (var i = 0; i < data.length; ++i) {
            var x = data[i];
            var delta = x - result.mean;
            var sweep = i + 1.0;
            result.mean += delta / sweep;
            squareSum += delta * (x - result.mean);
        }
        result.variance = data.length <= 1 ? 0 : squareSum / (data.length - 1);
        result.stdev = Math.sqrt(result.variance);
        result.unit = unit || "ms";

        return result;
    }

    PerfTestRunner.logStatistics = function (values, unit, title) {
        var statistics = this.computeStatistics(values, unit);
        this.log("");
        this.log(title);
        if (statistics.values)
            this.log("values " + statistics.values.join(", ") + " " + statistics.unit);
        this.log("avg " + statistics.mean + " " + statistics.unit);
        this.log("median " + statistics.median + " " + statistics.unit);
        this.log("stdev " + statistics.stdev + " " + statistics.unit);
        this.log("min " + statistics.min + " " + statistics.unit);
        this.log("max " + statistics.max + " " + statistics.unit);
    }

    function getUsedJSHeap() {
        return console.memory.usedJSHeapSize;
    }

    PerfTestRunner.gc = function () {
        if (window.GCController)
            window.GCController.collectAll();
        else {
            function gcRec(n) {
                if (n < 1)
                    return {};
                var temp = {i: "ab" + i + (i / 100000)};
                temp += "foo";
                gcRec(n-1);
            }
            for (var i = 0; i < 1000; i++)
                gcRec(10);
        }
    };

    function logInDocument(text) {
        if (!document.getElementById("log")) {
            var pre = document.createElement("pre");
            pre.id = "log";
            document.body.appendChild(pre);
        }
        document.getElementById("log").innerHTML += text + "\n";
        window.scrollTo(0, document.body.height);
    }

    PerfTestRunner.log = function (text) {
        if (logLines)
            logLines.push(text);
        else
            logInDocument(text);
    }

    PerfTestRunner.logFatalError = function (text) {
        PerfTestRunner.log("FATAL: " + text);
        finish();
    }

    PerfTestRunner.forceLayout = function(doc) {
        doc = doc || document;
        if (doc.body)
            doc.body.offsetHeight;
        else if (doc.documentElement)
            doc.documentElement.offsetHeight;
    };

    function start(test, runner) {
        if (!test) {
            PerfTestRunner.logFatalError("Got a bad test object.");
            return;
        }
        currentTest = test;
        // FIXME: We should be using multiple instances of test runner on Dromaeo as well but it's too slow now.
        // FIXME: Don't hard code the number of in-process iterations to use inside a test runner.
        iterationCount = test.dromaeoIterationCount || (window.testRunner ? 5 : 20);
        if (test.warmUpCount && test.warmUpCount > 0)
            completedIterations = -test.warmUpCount;
        logLines = window.testRunner ? [] : null;
        PerfTestRunner.log("Running " + iterationCount + " times");
        if (test.doNotIgnoreInitialRun)
            completedIterations++;
        if (runner)
            scheduleNextRun(runner);
    }

    function scheduleNextRun(runner) {
        PerfTestRunner.gc();
        window.setTimeout(function () {
            try {
                if (currentTest.setup)
                    currentTest.setup();

                var measuredValue = runner();
            } catch (exception) {
                PerfTestRunner.logFatalError("Got an exception while running test.run with name=" + exception.name + ", message=" + exception.message);
                return;
            }

            completedIterations++;

            try {
                ignoreWarmUpAndLog(measuredValue);
            } catch (exception) {
                PerfTestRunner.logFatalError("Got an exception while logging the result with name=" + exception.name + ", message=" + exception.message);
                return;
            }

            if (completedIterations < iterationCount)
                scheduleNextRun(runner);
            else
                finish();
        }, 0);
    }

    function ignoreWarmUpAndLog(measuredValue) {
        var labeledResult = measuredValue + " " + PerfTestRunner.unit;
        if (completedIterations <= 0)
            PerfTestRunner.log("Ignoring warm-up run (" + labeledResult + ")");
        else {
            results.push(measuredValue);
            if (window.internals && !currentTest.doNotMeasureMemoryUsage) {
                jsHeapResults.push(getUsedJSHeap());
            }
            PerfTestRunner.log(labeledResult);
        }
    }

    function finish() {
        try {
            if (currentTest.description)
                PerfTestRunner.log("Description: " + currentTest.description);
            PerfTestRunner.logStatistics(results, PerfTestRunner.unit, "Time:");
            if (jsHeapResults.length) {
                PerfTestRunner.logStatistics(jsHeapResults, "bytes", "JS Heap:");
            }
            if (logLines)
                logLines.forEach(logInDocument);
            if (currentTest.done)
                currentTest.done();
        } catch (exception) {
            logInDocument("Got an exception while finalizing the test with name=" + exception.name + ", message=" + exception.message);
        }

        if (window.testRunner)
            testRunner.notifyDone();
    }

    PerfTestRunner.prepareToMeasureValuesAsync = function (test) {
        PerfTestRunner.unit = test.unit;
        start(test);
    }

    PerfTestRunner.measureValueAsync = function (measuredValue) {
        completedIterations++;

        try {
            ignoreWarmUpAndLog(measuredValue);
        } catch (exception) {
            PerfTestRunner.logFatalError("Got an exception while logging the result with name=" + exception.name + ", message=" + exception.message);
            return;
        }

        if (completedIterations >= iterationCount)
            finish();
    }

    PerfTestRunner.measureTime = function (test) {
        PerfTestRunner.unit = "ms";
        start(test, measureTimeOnce);
    }

    function measureTimeOnce() {
        var start = PerfTestRunner.now();
        var returnValue = currentTest.run();
        var end = PerfTestRunner.now();

        if (returnValue - 0 === returnValue) {
            if (returnValue < 0)
                PerfTestRunner.log("runFunction returned a negative value: " + returnValue);
            return returnValue;
        }

        return end - start;
    }

    PerfTestRunner.measureRunsPerSecond = function (test) {
        PerfTestRunner.unit = "runs/s";
        start(test, measureRunsPerSecondOnce);
    }

    function measureRunsPerSecondOnce() {
        var timeToRun = 750;
        var totalTime = 0;
        var numberOfRuns = 0;

        while (totalTime < timeToRun) {
            totalTime += callRunAndMeasureTime(callsPerIteration);
            numberOfRuns += callsPerIteration;
            if (completedIterations < 0 && totalTime < 100)
                callsPerIteration = Math.max(10, 2 * callsPerIteration);
        }

        return numberOfRuns * 1000 / totalTime;
    }

    function callRunAndMeasureTime(callsPerIteration) {
        var startTime = PerfTestRunner.now();
        for (var i = 0; i < callsPerIteration; i++)
            currentTest.run();
        return PerfTestRunner.now() - startTime;
    }


    PerfTestRunner.measurePageLoadTime = function(test) {
        test.run = function() {
            var file = PerfTestRunner.loadFile(test.path);
            if (!test.chunkSize)
                this.chunkSize = 50000;

            var chunks = [];
            // The smaller the chunks the more style resolves we do.
            // Smaller chunk sizes will show more samples in style resolution.
            // Larger chunk sizes will show more samples in line layout.
            // Smaller chunk sizes run slower overall, as the per-chunk overhead is high.
            var chunkCount = Math.ceil(file.length / this.chunkSize);
            for (var chunkIndex = 0; chunkIndex < chunkCount; chunkIndex++) {
                var chunk = file.substr(chunkIndex * this.chunkSize, this.chunkSize);
                chunks.push(chunk);
            }

            PerfTestRunner.logInfo("Testing " + file.length + " byte document in " + chunkCount + " " + this.chunkSize + " byte chunks.");

            var iframe = document.createElement("iframe");
            document.body.appendChild(iframe);

            iframe.sandbox = '';  // Prevent external loads which could cause write() to return before completing the parse.
            iframe.style.width = "600px"; // Have a reasonable size so we're not line-breaking on every character.
            iframe.style.height = "800px";
            iframe.contentDocument.open();

            for (var chunkIndex = 0; chunkIndex < chunks.length; chunkIndex++) {
                iframe.contentDocument.write(chunks[chunkIndex]);
                PerfTestRunner.forceLayout(iframe.contentDocument);
            }

            iframe.contentDocument.close();
            document.body.removeChild(iframe);
        };

        PerfTestRunner.measureTime(test);
    }

    window.PerfTestRunner = PerfTestRunner;
})();
