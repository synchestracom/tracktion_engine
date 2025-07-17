/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             DemoRunner
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This example simply loads a project from the command line and plays it back in a loop.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine, tracktion_graph
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1
  defines:          JUCE_MODAL_LOOPS_PERMITTED=1

  type:             Component
  mainClass:        DemoRunner

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "../common/Utilities.h"

using namespace tracktion::literals;
using namespace std::literals;


// TODO better: put log utils elsewhere
#define SY_LOG(textToWrite) JUCE_BLOCK_WITH_FORCED_SEMICOLON (juce::String tempDbgBuf = juce::Time::getCurrentTime().toString(false, true) + " "; tempDbgBuf << textToWrite; juce::Logger::writeToLog (tempDbgBuf);)
#define SY_ERR(textToWrite) { SY_LOG("Error : \t" << textToWrite); jassertfalse; }
#define SY_SEP "\n=******************************************************************************************************************\n"
#define SY_LOG_SEP(textToWrite) SY_LOG(SY_SEP << textToWrite << SY_SEP)

#define DONT_SY_LOG(textToWrite) JUCE_BLOCK_WITH_FORCED_SEMICOLON (textToWrite;)
#define DONT_DBG(textToWrite) JUCE_BLOCK_WITH_FORCED_SEMICOLON (textToWrite;)


//==============================================================================
//==============================================================================
struct DemoType
{
    DemoType (const juce::String typeName, std::function<std::unique_ptr<Component> (te::Engine&)> createFunc)
        : type (typeName), create (std::move (createFunc))
    {}

    const juce::String type;
    std::function<std::unique_ptr<Component> (te::Engine&)> create;
};

//==============================================================================
struct DemoTypeManager
{
    static void registerDemo (std::unique_ptr<DemoType> type)
    {
        getTypes().push_back (std::move (type));
    }

    static std::vector<std::unique_ptr<DemoType>>& getTypes()
    {
        static std::vector<std::unique_ptr<DemoType>> types;
        return types;
    }

    static juce::StringArray getNames()
    {
        juce::StringArray names;

        for (auto& t : getTypes())
            names.add (t->type);

        return names;
    }

    static std::unique_ptr<juce::Component> createDemo (const juce::String& type, te::Engine& engine)
    {
        for (auto& t : getTypes())
            if (t->type == type)
                return t->create (engine);

        assert (false && "Invalid demo type");
        return {};
    }
};

//==============================================================================
template<typename Type>
struct DemoTypeBase
{
    DemoTypeBase (const juce::String& typeName)
    {
        DemoTypeManager::registerDemo (std::make_unique<DemoType> (typeName, [] (te::Engine& e) { return std::make_unique<Type> (e); }));
    }
};


//==============================================================================
//==============================================================================
// Include demo files to register them
#include "demos/AbletonLinkDemo.h"
//#include "demos/ContainerClipDemo.h"
#include "demos/DistortionEffectDemo.h"
#include "demos/GlobalQuantiseDemo.h"
#include "demos/IRPluginDemo.h"
#include "demos/MidiRecordingDemo.h"
#include "demos/PatternGeneratorDemo.h"
#include "demos/PitchAndTimeDemo.h"
#include "demos/PlaybackDemo.h"
#include "demos/PluginDemo.h"
#include "demos/RecordingDemo.h"
#include "demos/StepSequencerDemo.h"


//==============================================================================
//==============================================================================
class DemoRunner  : public Component
{
public:
    //==============================================================================
    DemoRunner()
    {
        
        fileLogger.reset(juce::FileLogger::createDateStampedLogger("DemoRunner logs", "DemoRunner-", "log", "DemoRunner logs"));
        juce::Logger::setCurrentLogger(fileLogger.get());
        
        Helpers::addAndMakeVisible (*this, { &loadButton, &pluginListButton, &audioSettingsButton });

        loadButton.onClick          = [this] { showLoadDemoMenu(); };

        // Show the plugin scan dialog
        // If you're loading an Edit with plugins in, you'll need to perform a scan first
        pluginListButton.onClick    = [this]
        {
            DialogWindow::LaunchOptions o;
            o.dialogTitle                   = TRANS("Plugins");
            o.dialogBackgroundColour        = Colours::black;
            o.escapeKeyTriggersCloseButton  = true;
            o.useNativeTitleBar             = true;
            o.resizable                     = true;
            o.useBottomRightCornerResizer   = true;

            auto v = new PluginListComponent (engine.getPluginManager().pluginFormatManager,
                                              engine.getPluginManager().knownPluginList,
                                              engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal"),
                                              te::getApplicationSettings());
            v->setSize (800, 600);
            o.content.setOwned (v);
            o.launchAsync();
        };
        audioSettingsButton.onClick = [this] { EngineHelpers::showAudioDeviceSettings (engine); };

        setSize (1200, 800);
        
        loadDemo("Audio Recording");
    }

    ~DemoRunner() override
    {
        engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
        juce::Logger::setCurrentLogger (nullptr);
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        const auto backgroundCol = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
        g.fillAll (backgroundCol);

        auto r = getLocalBounds();
        g.setColour (backgroundCol.darker());
        g.fillRect (r.removeFromTop (30));

        if (demo)
            return;

        g.setColour (getLookAndFeel().findColour (Label::textColourId));
        g.drawText ("Select a demo above to begin", r, juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        //auto topR = r.removeFromTop (30);
        //const int buttonW = topR.getWidth() / 3;
        //loadButton.setBounds (topR.removeFromLeft (buttonW).reduced (2));
        //pluginListButton.setBounds (topR.removeFromRight (buttonW / 2).reduced (2));
        //audioSettingsButton.setBounds (topR.removeFromRight (buttonW / 2).reduced (2));

        if (demo)
            demo->setBounds (r.reduced (2));
    }

private:
    //==============================================================================
    te::Engine engine { ProjectInfo::projectName, std::make_unique<ExtendedUIBehaviour>(), nullptr };

    TextButton loadButton { "Load Demo" }, pluginListButton { "Plugin List" }, audioSettingsButton { "Audio Settings" };
    std::unique_ptr<Component> demo;

    //==============================================================================
    void showLoadDemoMenu()
    {
        juce::PopupMenu m;

        for (auto type : DemoTypeManager::getNames())
            m.addItem (type, [this, type] { loadDemo (type); });

        m.showMenuAsync ({});
    }

    void loadDemo (const String& type)
    {
        if ((demo = DemoTypeManager::createDemo (type, engine)))
        {
            addAndMakeVisible (*demo);
            resized();
        }
    }
    
    std::unique_ptr<juce::FileLogger> fileLogger;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DemoRunner)
};
