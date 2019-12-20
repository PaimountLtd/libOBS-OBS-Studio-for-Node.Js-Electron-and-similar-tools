import 'mocha';
import { expect } from 'chai';
import * as osn from '../osn';
import { logInfo, logEmptyLine } from '../util/logger';
import { OBSHandler, IConfigProgress } from '../util/obs_handler';
import { deleteConfigFiles } from '../util/general';

const testName = 'nodeobs_autoconfig';

describe(testName, function() {
    let obs: OBSHandler;
    let hasTestFailed: boolean = false;

    // Initialize OBS process
    before(async function() {
        logInfo(testName, 'Starting ' + testName + ' tests');
        deleteConfigFiles();
        obs = new OBSHandler(testName);

        obs.instantiateUserPool(testName);

        // Reserving user from pool
        await obs.reserveUser();
    });

    // Shutdown OBS process
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
    });

    context('# Full auto config run', function() {
        it('Run autoconfig successfully', async function() {
            let progressInfo: IConfigProgress;
            let settingValue: any;

            obs.startAutoconfig();

            osn.NodeObs.StartBandwidthTest();

            progressInfo = await obs.getNextProgressInfo('Bandwidth test');

            if (progressInfo.event != 'error') {
                expect(progressInfo.event).to.equal('stopping_step');
                expect(progressInfo.description).to.equal('bandwidth_test');
                expect(progressInfo.percentage).to.equal(100);

                osn.NodeObs.StartStreamEncoderTest();

                progressInfo = await obs.getNextProgressInfo('Stream Encoder test');
                expect(progressInfo.event).to.equal('stopping_step');
                expect(progressInfo.description).to.equal('streamingEncoder_test');
                expect(progressInfo.percentage).to.equal(100);

                osn.NodeObs.StartRecordingEncoderTest();

                progressInfo = await obs.getNextProgressInfo('Recording Encoder test');
                expect(progressInfo.event).to.equal('stopping_step');
                expect(progressInfo.description).to.equal('recordingEncoder_test');
                expect(progressInfo.percentage).to.equal(100);

                osn.NodeObs.StartCheckSettings();

                progressInfo = await obs.getNextProgressInfo('Check Settings');
                expect(progressInfo.event).to.equal('stopping_step');
                expect(progressInfo.description).to.equal('checking_settings');
                expect(progressInfo.percentage).to.equal(100);

                osn.NodeObs.StartSaveStreamSettings();

                progressInfo = await obs.getNextProgressInfo('Save Stream Settings');
                expect(progressInfo.event).to.equal('stopping_step');
                expect(progressInfo.description).to.equal('saving_service');
                expect(progressInfo.percentage).to.equal(100);

                osn.NodeObs.StartSaveSettings();

                progressInfo = await obs.getNextProgressInfo('Save Settings');
                expect(progressInfo.event).to.equal('stopping_step');
                expect(progressInfo.description).to.equal('saving_settings');
                expect(progressInfo.percentage).to.equal(100);

                progressInfo = await obs.getNextProgressInfo('Autoconfig done');
                expect(progressInfo.event).to.equal('done');
            } else {
                osn.NodeObs.StartSetDefaultSettings();

                progressInfo = await obs.getNextProgressInfo('Set Default Settings');
                expect(progressInfo.event).to.equal('stopping_step');
                expect(progressInfo.description).to.equal('setting_default_settings');
                expect(progressInfo.percentage).to.equal(100);

                osn.NodeObs.StartSaveStreamSettings();

                progressInfo = await obs.getNextProgressInfo('Save Stream Settings');
                expect(progressInfo.event).to.equal('stopping_step');
                expect(progressInfo.description).to.equal('saving_service');
                expect(progressInfo.percentage).to.equal(100);

                osn.NodeObs.StartSaveSettings();

                progressInfo = await obs.getNextProgressInfo('Save Settings');
                expect(progressInfo.event).to.equal('stopping_step');
                expect(progressInfo.description).to.equal('saving_settings');
                expect(progressInfo.percentage).to.equal(100);

                progressInfo = await obs.getNextProgressInfo('Autoconfig done');
                expect(progressInfo.event).to.equal('done');

                // Checking default settings
                settingValue = obs.getSetting('Output', 'Mode');
                expect(settingValue).to.equal('Simple');

                settingValue = obs.getSetting('Output', 'VBitrate');
                expect(settingValue).to.equal(2500);

                settingValue = obs.getSetting('Output', 'StreamEncoder');
                expect(settingValue).to.equal('x264');

                settingValue = obs.getSetting('Output', 'RecQuality');
                expect(settingValue).to.equal('Small');

                settingValue = obs.getSetting('Advanced', 'DynamicBitrate');
                expect(settingValue).to.equal(false);

                settingValue = obs.getSetting('Video', 'Output');
                expect(settingValue).to.equal('1280x720');

                settingValue = obs.getSetting('Video', 'FPSType');
                expect(settingValue).to.equal('Common FPS Values');

                settingValue = obs.getSetting('Video', 'FPSCommon');
                expect(settingValue).to.equal('30');
            }

            osn.NodeObs.TerminateAutoConfig();
        });
    });
});
