import 'mocha'
import { expect } from 'chai'
import * as osn from '../osn';
import { logInfo, logEmptyLine } from '../util/logger';
import { OBSHandler } from '../util/obs_handler'
import { deleteConfigFiles } from '../util/general';

const testName = 'osn-fader';

describe(testName, () => {
    let obs: OBSHandler;
    let hasTestFailed: boolean = false;

    // Initialize OBS process
    before(function() {
        logInfo(testName, 'Starting ' + testName + ' tests');
        deleteConfigFiles();
        obs = new OBSHandler(testName);
    });

    // Shutdown OBS process
    after(async function() {
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

    context('# Create and Attach', () => {
        it('Create cubic fader and attach it to a audio source', () => {
            // Creating audio source
            const input = osn.InputFactory.create('wasapi_input_capture', 'input');

            // Checking if input source was created correctly
            expect(input).to.not.equal(undefined);
            expect(input.id).to.equal('wasapi_input_capture');
            expect(input.name).to.equal('input');

            // Creating fader
            const cubicFader = osn.FaderFactory.create(osn.EFaderType.Cubic);

            // Checking if fader was created correctly
            expect(cubicFader).to.not.equal(undefined);

            // Attach fader to input source
            expect(function() {
                cubicFader.attach(input);
            }).to.not.throw();
            
            input.release();
        });

        it('Create IEC fader and attach it to a audio source', () => {
            // Creating audio source
            const input = osn.InputFactory.create('wasapi_input_capture', 'input');

            // Checking if input source was created correctly
            expect(input).to.not.equal(undefined);
            expect(input.id).to.equal('wasapi_input_capture');
            expect(input.name).to.equal('input');

            // Creating fader
            const iecFader = osn.FaderFactory.create(osn.EFaderType.IEC);

            // Checking if fader was created correctly
            expect(iecFader).to.not.equal(undefined);

            // Attach fader to input source
            expect(function() {
                iecFader.attach(input);
            }).to.not.throw();
            
            input.release();
        });

        it('Create logarithmic fader and attach it to a audio source', () => {
            // Creating audio source
            const input = osn.InputFactory.create('wasapi_input_capture', 'input');

            // Checking if input source was created correctly
            expect(input).to.not.equal(undefined);
            expect(input.id).to.equal('wasapi_input_capture');
            expect(input.name).to.equal('input');

            // Creating fader
            const logFader = osn.FaderFactory.create(osn.EFaderType.Log);

            // Checking if fader was created correctly
            expect(logFader).to.not.equal(undefined);

            // Attach fader to input source
            expect(function() {
                logFader.attach(input);
            }).to.not.throw();
            
            input.release();
        });
    });

    context('# GetDeciBel, GetDeflection and GetMultiplier', () => {
        it('Get decibel, deflection and multiplier values', () => {
            let faderTypes: osn.EFaderType[] = [osn.EFaderType.Cubic, osn.EFaderType.IEC, osn.EFaderType.Log];

            faderTypes.forEach(function(faderType) {
                // Creating fader
                const fader = osn.FaderFactory.create(faderType);

                // Checking if fader was created correctly
                expect(fader).to.not.equal(undefined);

                // Getting fader decibel
                const decibel = fader.db;

                // Checking if decibel value was returned correctly
                expect(decibel).to.not.equal(undefined);
                expect(decibel).to.equal(0);

                // Getting fader deflection
                const deflection = fader.deflection;

                // Checking if deflection value was returned correctly
                expect(deflection).to.not.equal(undefined);
                expect(deflection).to.equal(1);

                // Getting multiplier decibel
                const multiplier = fader.mul;

                // Checking if multiplier value was returned correctly
                expect(multiplier).to.not.equal(undefined);
                expect(multiplier).to.equal(1);
            });
        });
    });
});
