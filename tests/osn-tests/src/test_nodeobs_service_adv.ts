import 'mocha';
import { expect } from 'chai';
import * as osn from '../osn';
import { logInfo, logEmptyLine } from '../util/logger';
import { ETestErrorMsg, GetErrorMessage } from '../util/error_messages';
import { OBSHandler, IOBSOutputSignalInfo } from '../util/obs_handler';
import { deleteConfigFiles, sleep } from '../util/general';
import { EOBSOutputType, EOBSOutputSignal, EOBSSettingsCategories } from '../util/obs_enums';

const testName = 'nodeobs_service';

describe(testName, function() {
    let obs: OBSHandler;
    let hasTestFailed: boolean = false;
    const path = require('path');

    before(async function() {
        logInfo(testName, 'Starting ' + testName + ' tests');
        deleteConfigFiles();
        obs = new OBSHandler(testName);

        obs.instantiateUserPool(testName);

        // Reserving user from pool
        await obs.reserveUser();

        // Connecting output signals
        obs.connectOutputSignals();
    });

    after(async function() {
        // Releasing user got from pool
        await obs.releaseUser();

        // Closing OBS process
        obs.shutdown();

        if (hasTestFailed === true) {
            logInfo(testName, 'One or more test cases failed. Uploading cache');
            await obs.uploadTestCache();
        }

        obs = null;
        deleteConfigFiles();
        logInfo(testName, 'Finished ' + testName + ' tests');
        logEmptyLine();
    });

    afterEach(function() {
        if (this.currentTest.state == 'failed') {
            hasTestFailed = true;
        }
        sleep(500);
    });

    beforeEach(async function() {
    /*    const test_urls = [
            'rtmp://sea.contribute.live-video.net/app', 
            'rtmp://pdx.contribute.live-video.net/app',
            'rtmp://slc.contribute.live-video.net/app',
            'rtmp://sjc05.contribute.live-video.net/app',
            'rtmp://sjc02.contribute.live-video.net/app',
            'rtmp://lax.contribute.live-video.net/app',
            'rtmp://den52.contribute.live-video.net/app',
            'rtmp://phx.contribute.live-video.net/app',
        ];
        var test_url = test_urls[Math.floor(Math.random()*test_urls.length)];
        osn.ServiceFactory.serviceContext.update({ url: test_url });*/
/*
        let currentVideo = osn.Video.videoContext;
        const newVideoContext: osn.VideoContext = {
            fpsNum: 15,
            fpsDen: 1,
            baseWidth: 128,
            baseHeight: 128,
            outputWidth: 128,
            outputHeight: 128,
            outputFormat: osn.EVideoFormat.I420,
            colorspace: osn.EColorSpace.Default,
            range: osn.ERangeType.Default,
            scaleType: osn.EScaleType.Bicubic,
        };
        osn.Video.videoContext = newVideoContext;
        obs.setSetting(EOBSSettingsCategories.Output, 'bitrate', 256);*/
        sleep(500);
    });
/*
    it('Advanced mode - Start and stop streaming', async function() {
        // Preparing environment
        obs.setSetting(EOBSSettingsCategories.Output, 'Mode', 'Advanced');
        obs.setSetting(EOBSSettingsCategories.Output, 'Encoder', 'obs_x264');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecEncoder', 'none');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecFilePath', path.join(path.normalize(__dirname), '..', 'osnData'));

        let signalInfo: IOBSOutputSignalInfo;

        osn.NodeObs.OBS_service_startStreaming();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Starting);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Starting, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Activate);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            throw Error(GetErrorMessage(ETestErrorMsg.StreamOutputDidNotStart, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Activate, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Start);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.StreamOutput));

        await sleep(500);

        osn.NodeObs.OBS_service_stopStreaming(false);

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Stopping);

        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            throw Error(GetErrorMessage(ETestErrorMsg.StreamOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Deactivate);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Deactivate, GetErrorMessage(ETestErrorMsg.StreamOutput));
    });
*/
    it('Advanced mode - Start recording and stop', async function() {
        // Preparing environment
        obs.setSetting(EOBSSettingsCategories.Output, 'Mode', 'Advanced');
        obs.setSetting(EOBSSettingsCategories.Output, 'Encoder', 'obs_x264');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecEncoder', 'none');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecFilePath', path.join(path.normalize(__dirname), '..', 'osnData'));

        let signalInfo: IOBSOutputSignalInfo;

        osn.NodeObs.OBS_service_startRecording();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Start);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            throw Error(GetErrorMessage(ETestErrorMsg.RecordOutputDidNotStart, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        await sleep(500);

        osn.NodeObs.OBS_service_stopRecording();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Stopping);
        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            throw Error(GetErrorMessage(ETestErrorMsg.RecordOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Wrote);

        if (signalInfo.code != 0) {
            throw Error(GetErrorMessage(ETestErrorMsg.RecordOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Wrote, GetErrorMessage(ETestErrorMsg.RecordingOutput));
    });

    it('Advanced mode - Start replay buffer, save replay and stop', async function() {
        // Preparing environment
        obs.setSetting(EOBSSettingsCategories.Output, 'Mode', 'Advanced');
        obs.setSetting(EOBSSettingsCategories.Output, 'Encoder', 'obs_x264');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecEncoder', 'none');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecFilePath', path.join(path.normalize(__dirname), '..', 'osnData'));

        let signalInfo: IOBSOutputSignalInfo;

        osn.NodeObs.OBS_service_startReplayBuffer();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Start);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            throw Error(GetErrorMessage(ETestErrorMsg.ReplayBufferDidNotStart, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        await sleep(500);

        osn.NodeObs.OBS_service_processReplayBufferHotkey();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Writing);
        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Writing, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Wrote);
        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Wrote, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        osn.NodeObs.OBS_service_stopReplayBuffer(false);

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Stopping);
        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            throw Error(GetErrorMessage(ETestErrorMsg.ReplayBufferStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
    });
/*
    it('Advanced mode - Record while streaming', async function() {
        // Preparing environment
        obs.setSetting(EOBSSettingsCategories.Output, 'Mode', 'Advanced');
        obs.setSetting(EOBSSettingsCategories.Output, 'Encoder', 'obs_x264');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecEncoder', 'none');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecFilePath', path.join(path.normalize(__dirname), '..', 'osnData'));

        let signalInfo: IOBSOutputSignalInfo;

        osn.NodeObs.OBS_service_startStreaming();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Starting);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Starting, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Activate);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            throw Error(GetErrorMessage(ETestErrorMsg.StreamOutputDidNotStart, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Activate, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Start);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.StreamOutput));

        osn.NodeObs.OBS_service_startRecording();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Start);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.RecordOutputDidNotStart, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        await sleep(500);

        osn.NodeObs.OBS_service_stopRecording();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Stopping);
        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.RecordOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Wrote);

        if (signalInfo.code != 0) {
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.RecordOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Wrote, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        osn.NodeObs.OBS_service_stopStreaming(false);

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Stopping);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            throw Error(GetErrorMessage(ETestErrorMsg.StreamOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Deactivate);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Deactivate, GetErrorMessage(ETestErrorMsg.StreamOutput));
    });

    it('Advanced mode - Record replay while streaming and save', async function() {
        // Preparing environment
        obs.setSetting(EOBSSettingsCategories.Output, 'Mode', 'Advanced');
        obs.setSetting(EOBSSettingsCategories.Output, 'Encoder', 'obs_x264');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecEncoder', 'none');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecFilePath', path.join(path.normalize(__dirname), '..', 'osnData'));

        let signalInfo: IOBSOutputSignalInfo;

        osn.NodeObs.OBS_service_startStreaming();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Starting);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Starting, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Activate);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            throw Error(GetErrorMessage(ETestErrorMsg.StreamOutputDidNotStart, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Activate, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Start);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.StreamOutput));

        osn.NodeObs.OBS_service_startReplayBuffer();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Start);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.ReplayBufferDidNotStart, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        await sleep(500);

        osn.NodeObs.OBS_service_processReplayBufferHotkey();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Writing);
        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Writing, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Wrote);
        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Wrote, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        osn.NodeObs.OBS_service_stopReplayBuffer(false);

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Stopping);
        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.ReplayBufferStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        osn.NodeObs.OBS_service_stopStreaming(false);

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Stopping);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            throw Error(GetErrorMessage(ETestErrorMsg.StreamOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Deactivate);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Deactivate, GetErrorMessage(ETestErrorMsg.StreamOutput));
    });

    it('Advanced mode - Record and use replay buffer while streaming', async function() {
        // Preparing environment
        obs.setSetting(EOBSSettingsCategories.Output, 'Mode', 'Advanced');
        obs.setSetting(EOBSSettingsCategories.Output, 'Encoder', 'obs_x264');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecEncoder', 'none');
        obs.setSetting(EOBSSettingsCategories.Output, 'RecFilePath', path.join(path.normalize(__dirname), '..', 'osnData'));

        let signalInfo: IOBSOutputSignalInfo;

        osn.NodeObs.OBS_service_startStreaming();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Starting);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Starting, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Activate);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            throw Error(GetErrorMessage(ETestErrorMsg.StreamOutputDidNotStart, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Activate, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Start);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.StreamOutput));

        osn.NodeObs.OBS_service_startRecording();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Start);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.RecordOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        osn.NodeObs.OBS_service_startReplayBuffer();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Start);

        if (signalInfo.signal == EOBSOutputSignal.Stop) {
            osn.NodeObs.OBS_service_stopRecording();
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.ReplayBufferDidNotStart, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Start, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        await sleep(500);

        osn.NodeObs.OBS_service_processReplayBufferHotkey();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Writing);
        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Writing, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Wrote);
        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Wrote, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        osn.NodeObs.OBS_service_stopReplayBuffer(false);

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Stopping);
        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.ReplayBuffer, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            osn.NodeObs.OBS_service_stopRecording();
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.ReplayBufferStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.ReplayBuffer, GetErrorMessage(ETestErrorMsg.ReplayBuffer));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.ReplayBuffer));

        osn.NodeObs.OBS_service_stopRecording();

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Stopping);
        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.RecordOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Recording, EOBSOutputSignal.Wrote);

        if (signalInfo.code != 0) {
            osn.NodeObs.OBS_service_stopStreaming(false);
            throw Error(GetErrorMessage(ETestErrorMsg.RecordOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Recording, GetErrorMessage(ETestErrorMsg.RecordingOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Wrote, GetErrorMessage(ETestErrorMsg.RecordingOutput));

        osn.NodeObs.OBS_service_stopStreaming(false);

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Stopping);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stopping, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Stop);

        if (signalInfo.code != 0) {
            throw Error(GetErrorMessage(ETestErrorMsg.StreamOutputStoppedWithError, signalInfo.code.toString(), signalInfo.error));
        }

        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Stop, GetErrorMessage(ETestErrorMsg.StreamOutput));

        signalInfo = await obs.getNextSignalInfo(EOBSOutputType.Streaming, EOBSOutputSignal.Deactivate);
        expect(signalInfo.type).to.equal(EOBSOutputType.Streaming, GetErrorMessage(ETestErrorMsg.StreamOutput));
        expect(signalInfo.signal).to.equal(EOBSOutputSignal.Deactivate, GetErrorMessage(ETestErrorMsg.StreamOutput));
    });
 */
});
