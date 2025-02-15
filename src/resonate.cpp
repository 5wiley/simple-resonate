// Copyright 2015 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

#include "dsp/part.h"
#include "dsp/strummer.h"
#include "dsp/string_synth_part.h"
#include "cv_scaler.h"
#include "daisy_patch.h"
#include "UiHardware.h"

using namespace daisy;
using namespace resonate;

daisy::UI ui;

FullScreenItemMenu mainMenu;
FullScreenItemMenu controlEditMenu;
FullScreenItemMenu polyEditMenu;
FullScreenItemMenu boolEditMenu;
FullScreenItemMenu normEditMenu;
UiEventQueue       eventQueue;
DaisyPatch         hw;

uint16_t reverb_buffer[32768];

CvScaler        cv_scaler;
Part            part;
StringSynthPart string_synth;
Strummer        strummer;

const int                kNumMainMenuItems = 4;
AbstractMenu::ItemConfig mainMenuItems[kNumMainMenuItems];
const int                kNumControlEditMenuItems = 5;
AbstractMenu::ItemConfig controlEditMenuItems[kNumControlEditMenuItems];
const int                kNumPolyEditMenuItems = 4;
AbstractMenu::ItemConfig polyEditMenuItems[kNumPolyEditMenuItems];
const int                kNumNormEditMenuItems = 4;
AbstractMenu::ItemConfig normEditMenuItems[kNumNormEditMenuItems];

// control menu items
const char* controlListValues[]
    = {"Frequency", "Structure", "Brightness", "Damping", "Position"};
MappedStringListValue controlListValueOne(controlListValues, 5, 0);
MappedStringListValue controlListValueTwo(controlListValues, 5, 1);
MappedStringListValue controlListValueThree(controlListValues, 5, 2);
MappedStringListValue controlListValueFour(controlListValues, 5, 3);

// poly/model menu items
const char*           polyListValues[] = {"One", "Two", "Four"};
MappedStringListValue polyListValue(polyListValues, 3, 0);
const char*           modelListValues[] = {"Modal Synthesis",
                                 "SYMP",
                                 "INHR",
                                 "FM",
                                 "CHRD",
                                 "STRVB"};
MappedStringListValue modelListValue(modelListValues, 6, 0);
const char*           eggListValues[]
    = {"Formant", "Chorus", "Reverb", "Bi-Formant", "Ensemble", "Cathedral"};
MappedStringListValue eggListValue(eggListValues, 6, 0);

// norm edit menu items
bool exciterIn;
bool strumIn;
bool noteIn;

//easter egg toggle
bool deviled_eggs;

int oldModel = 0;

void InitUi()
{
    UI::SpecialControlIds specialControlIds;
    specialControlIds.okBttnId
        = bttnEncoder; // Encoder button is our okay button
    specialControlIds.menuEncoderId
        = encoderMain; // Encoder is used as the main menu navigation encoder

    // This is the canvas for the OLED display.
    UiCanvasDescriptor oledDisplayDescriptor;
    oledDisplayDescriptor.id_     = canvasOledDisplay; // the unique ID
    oledDisplayDescriptor.handle_ = &hw.display;   // a pointer to the display
    oledDisplayDescriptor.updateRateMs_      = 50; // 50ms == 20Hz
    oledDisplayDescriptor.screenSaverTimeOut = 0;  // display always on
    oledDisplayDescriptor.clearFunction_     = &ClearCanvas;
    oledDisplayDescriptor.flushFunction_     = &FlushCanvas;

    ui.Init(eventQueue,
            specialControlIds,
            {oledDisplayDescriptor},
            canvasOledDisplay);
}

void InitUiPages()
{
    // ====================================================================
    // The main menu
    // ====================================================================

    mainMenuItems[0].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[0].text = "Controls";
    mainMenuItems[0].asOpenUiPageItem.pageToOpen = &controlEditMenu;

    mainMenuItems[1].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[1].text = "Mode";
    mainMenuItems[1].asOpenUiPageItem.pageToOpen = &polyEditMenu;

    mainMenuItems[2].type = daisy::AbstractMenu::ItemType::openUiPageItem;
    mainMenuItems[2].text = "Normalize";
    mainMenuItems[2].asOpenUiPageItem.pageToOpen = &normEditMenu;

    mainMenuItems[3].type = daisy::AbstractMenu::ItemType::checkboxItem;
    mainMenuItems[3].text = "Easter Egg";
    mainMenuItems[3].asCheckboxItem.valueToModify = &deviled_eggs;

    mainMenu.Init(mainMenuItems, kNumMainMenuItems);

    // ====================================================================
    // The "control edit" menu
    // ====================================================================

    controlEditMenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    controlEditMenuItems[0].text = "Knob One";
    controlEditMenuItems[0].asMappedValueItem.valueToModify
        = &controlListValueOne;

    controlEditMenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;
    controlEditMenuItems[1].text = "Knob Two";
    controlEditMenuItems[1].asMappedValueItem.valueToModify
        = &controlListValueTwo;

    controlEditMenuItems[2].type = daisy::AbstractMenu::ItemType::valueItem;
    controlEditMenuItems[2].text = "Knob Three";
    controlEditMenuItems[2].asMappedValueItem.valueToModify
        = &controlListValueThree;

    controlEditMenuItems[3].type = daisy::AbstractMenu::ItemType::valueItem;
    controlEditMenuItems[3].text = "Knob Four";
    controlEditMenuItems[3].asMappedValueItem.valueToModify
        = &controlListValueFour;

    controlEditMenuItems[4].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    controlEditMenuItems[4].text = "Back";

    controlEditMenu.Init(controlEditMenuItems, kNumControlEditMenuItems);

    // ====================================================================
    // The "poly edit" menu
    // ====================================================================
    polyEditMenuItems[0].type = daisy::AbstractMenu::ItemType::valueItem;
    polyEditMenuItems[0].text = "Polyphony";
    polyEditMenuItems[0].asMappedValueItem.valueToModify = &polyListValue;

    polyEditMenuItems[1].type = daisy::AbstractMenu::ItemType::valueItem;
    polyEditMenuItems[1].text = "Model";
    polyEditMenuItems[1].asMappedValueItem.valueToModify = &modelListValue;

    polyEditMenuItems[2].type = daisy::AbstractMenu::ItemType::valueItem;
    polyEditMenuItems[2].text = "Effects";
    polyEditMenuItems[2].asMappedValueItem.valueToModify = &eggListValue;

    polyEditMenuItems[3].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    polyEditMenuItems[3].text = "Back";

    polyEditMenu.Init(polyEditMenuItems, kNumPolyEditMenuItems);

    // ====================================================================
    // The "normalization edit" menu
    // ====================================================================
    normEditMenuItems[0].type = daisy::AbstractMenu::ItemType::checkboxItem;
    normEditMenuItems[0].text = "Exciter";
    normEditMenuItems[0].asCheckboxItem.valueToModify = &exciterIn;

    normEditMenuItems[1].type = daisy::AbstractMenu::ItemType::checkboxItem;
    normEditMenuItems[1].text = "Strum";
    normEditMenuItems[1].asCheckboxItem.valueToModify = &strumIn;

    normEditMenuItems[2].type = daisy::AbstractMenu::ItemType::checkboxItem;
    normEditMenuItems[2].text = "Note";
    normEditMenuItems[2].asCheckboxItem.valueToModify = &noteIn;

    normEditMenuItems[3].type = daisy::AbstractMenu::ItemType::closeMenuItem;
    normEditMenuItems[3].text = "Back";

    normEditMenu.Init(normEditMenuItems, kNumNormEditMenuItems);
}

void GenerateUiEvents()
{
    if(hw.encoder.RisingEdge())
        eventQueue.AddButtonPressed(bttnEncoder, 1);

    if(hw.encoder.FallingEdge())
        eventQueue.AddButtonReleased(bttnEncoder);

    const auto increments = hw.encoder.Increment();
    if(increments != 0)
        eventQueue.AddEncoderTurned(encoderMain, increments, 12);
}

int old_poly = 0;

void ProcessControls(Patch* patch, PerformanceState* state)
{
    // control settings
    cv_scaler.channel_map_[0] = controlListValueOne.GetIndex();
    cv_scaler.channel_map_[1] = controlListValueTwo.GetIndex();
    cv_scaler.channel_map_[2] = controlListValueThree.GetIndex();
    cv_scaler.channel_map_[3] = controlListValueFour.GetIndex();

    //polyphony setting
    int poly = polyListValue.GetIndex();
    if(old_poly != poly)
    {
        part.set_polyphony(0x01 << poly);
        string_synth.set_polyphony(0x01 << poly);
    }
    old_poly = poly;

    //model settings
    part.set_model((resonate::ResonatorModel)modelListValue.GetIndex());
    string_synth.set_fx((resonate::FxType)eggListValue.GetIndex());

    // normalization settings
    state->internal_note    = !noteIn;
    state->internal_exciter = !exciterIn;
    state->internal_strum   = !strumIn;

    //strum
    state->strum = hw.gate_input[0].Trig();
}

float input[kMaxBlockSize];
float output[kMaxBlockSize];
float aux[kMaxBlockSize];

const float kNoiseGateThreshold = 0.00003f;
float       in_level            = 0.0f;

#include <stdlib.h>
#include <math.h>

static float drone_current = 0.0f;
static float drone_target = 0.0f;
const float kDroneSlewRate = 0.0005f;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAllControls();
    GenerateUiEvents();

    PerformanceState performance_state;
    Patch            patch;

    ProcessControls(&patch, &performance_state);
    cv_scaler.Read(&patch, &performance_state);

    if(deviled_eggs)
    {
        for(size_t i = 0; i < size; ++i)
        {
            input[i] = in[0][i];
        }
        if (fabs(drone_current - drone_target) < 0.001f) {
            drone_target = -1.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX/2.0f));
        }
        drone_current += (drone_target - drone_current) * kDroneSlewRate;
        float modValue = 1.0f + 0.5f * drone_current;
        for(size_t i = 0; i < size; i++) {
            input[i] *= modValue;
        }
        strummer.Process(NULL, size, &performance_state);
        string_synth.Process(
            performance_state, patch, input, output, aux, size);
    }
    else
    {
        // The normal branch: apply noise gate and process as usual.
        for(size_t i = 0; i < size; i++)
        {
            float in_sample = in[0][i];
            float error, gain;
            error = in_sample * in_sample - in_level;
            in_level += error * (error > 0.0f ? 0.1f : 0.0001f);
            gain = in_level <= kNoiseGateThreshold
                       ? (1.0f / kNoiseGateThreshold) * in_level
                       : 1.0f;
            input[i] = gain * in_sample;
        }
        strummer.Process(input, size, &performance_state);
        part.Process(performance_state, patch, input, output, aux, size);
    }

    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = output[i];
        out[1][i] = aux[i];
    }
}
int main(void)
{
    hw.Init();
    float samplerate = hw.AudioSampleRate();
    float blocksize  = hw.AudioBlockSize();

    InitUi();
    InitUiPages();
    InitResources();
    ui.OpenPage(mainMenu);

    strummer.Init(0.01f, samplerate / blocksize);
    part.Init(reverb_buffer);
    string_synth.Init(reverb_buffer);

    cv_scaler.Init();

    UI::SpecialControlIds ids;

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while(1)
    {
        ui.Process();
    }
}
