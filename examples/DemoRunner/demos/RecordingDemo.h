/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../common/Utilities.h"
#include "../common/Components.h"

//==============================================================================
class RecordingDemo  : public Component,
                       private ChangeListener
{
public:
    //==============================================================================
    RecordingDemo (te::Engine& e)
        : engine (e)
    {
        newEditButton.onClick = [this] { createOrLoadEdit(); };
        importMidiButton.onClick = [this] {
            FileChooser fc ("Import Midi", File::getSpecialLocation (File::userDocumentsDirectory), "*.mid");
            if (fc.browseForFileToOpen())
            {
                int targetTrackIndex = 0;
                tracktion::Clipboard::pasteMIDIFileIntoEdit(*edit, fc.getResult(), targetTrackIndex, tracktion::TimePosition::fromSeconds(0), true);
                te::EditFileOperations (*edit).save (true, true, false);
            }
            else
                return;
        };
        
        reloadButton.onClick = [this] {
            createOrLoadEdit (editFile);
        };
        
        updatePlayButtonText();
        updateRecordButtonText();
        editNameLabel.setJustificationType (Justification::centred);
        Helpers::addAndMakeVisible (*this, { &newEditButton, &playPauseButton, &recordButton, &showEditButton,
                                             &newTrackButton, &clearTracksButton, &deleteButton, &editNameLabel, &showWaveformButton, &undoButton, &redoButton, &importMidiButton, &reloadButton });

        deleteButton.setEnabled (false);
        
        auto d = File::getSpecialLocation (File::tempDirectory).getChildFile ("RecordingDemo");
        d.createDirectory();
        
        if (editFile.existsAsFile())
            createOrLoadEdit (editFile);
        else
            createOrLoadEdit (d.getNonexistentChildFile ("Test", ".tracktionedit", false));
        
        selectionManager.addChangeListener (this);
        
        setupButtons();
        
        setSize (700, 500);
    }

    ~RecordingDemo() override
    {
        te::EditFileOperations (*edit).save (true, true, false);
        engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        int w = r.getWidth() / 5;
        auto topR = r.removeFromTop (30);
        //newEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        playPauseButton.setBounds (topR.removeFromLeft (w).reduced (2));
        recordButton.setBounds (topR.removeFromLeft (w).reduced (2));
        showEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        //newTrackButton.setBounds (topR.removeFromLeft (w).reduced (2));
        //clearTracksButton.setBounds (topR.removeFromLeft (w).reduced (2));
        //deleteButton.setBounds (topR.removeFromLeft (w).reduced (2));
        //undoButton.setBounds(topR.removeFromLeft(w).reduced(2));
        //redoButton.setBounds(topR.removeFromLeft(w).reduced(2));
        importMidiButton.setBounds(topR.removeFromLeft(w).reduced(2));
        reloadButton.setBounds(topR.removeFromLeft(w).reduced(2));

        topR = r.removeFromTop (30);
        showWaveformButton.setBounds (topR.removeFromLeft (w * 2).reduced (2));
        editNameLabel.setBounds (topR);
        
        if (editComponent != nullptr)
            editComponent->setBounds (r);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<EditComponent> editComponent;
    juce::File editFile {"/Users/mickael/Library/Synchestra/Pieces/Ravel - Bolero/Ravel - Bolero - import midi tempo.tracktionedit"};

    TextButton newEditButton { "New" }, playPauseButton { "Play" }, recordButton { "Record" },
               showEditButton { "Show Edit" }, newTrackButton { "New Track" }, clearTracksButton { "Clear Tracks" }, deleteButton { "Delete" },
               undoButton {"Undo"}, redoButton {"Redo"}, importMidiButton {"Import Midi"}, reloadButton {"Reload Edit"}, saveButton {"Save Edit"};
    Label editNameLabel { "No Edit Loaded" };
    ToggleButton showWaveformButton { "Show Waveforms" };

    //==============================================================================
    void setupButtons()
    {
        playPauseButton.onClick = [this]
        {
            bool wasRecording = edit->getTransport().isRecording();
            EngineHelpers::togglePlay (*edit);
            if (wasRecording)
                te::EditFileOperations (*edit).save (true, true, false);
        };
        recordButton.onClick = [this]
        {
            bool wasRecording = edit->getTransport().isRecording();
            EngineHelpers::toggleRecord (*edit);
            if (wasRecording)
                te::EditFileOperations (*edit).save (true, true, false);
        };
        newTrackButton.onClick = [this]
        {
            edit->ensureNumberOfAudioTracks (getAudioTracks (*edit).size() + 1);
        };
        clearTracksButton.onClick = [this]
        {
            for (auto t : te::getAudioTracks (*edit))
                edit->deleteTrack (t);
                
        };
        deleteButton.onClick = [this]
        {
            auto sel = selectionManager.getSelectedObject (0);
            if (auto clip = dynamic_cast<te::Clip*> (sel))
            {
                clip->removeFromParent();
            }
            else if (auto track = dynamic_cast<te::Track*> (sel))
            {
                if (! (track->isMasterTrack() || track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()))
                    edit->deleteTrack (track);
            }
        };
        showWaveformButton.onClick = [this]
        {
            auto& evs = editComponent->getEditViewState();
            evs.drawWaveforms = ! evs.drawWaveforms.get();
            showWaveformButton.setToggleState (evs.drawWaveforms, dontSendNotification);
        };
        undoButton.onClick = [this]
        {
            edit->getUndoManager().undo();
        };
        redoButton.onClick = [this]
        {
            edit->getUndoManager().redo();
        };
    }
    
    void updatePlayButtonText()
    {
        if (edit != nullptr)
            playPauseButton.setButtonText (edit->getTransport().isPlaying() ? "Stop" : "Play");
    }
    
    void updateRecordButtonText()
    {
        if (edit != nullptr)
            recordButton.setButtonText (edit->getTransport().isRecording() ? "Abort" : "Record");
    }

    void createOrLoadEdit (File newEditFile = {})
    {
        editFile = newEditFile;
        if (editFile == File())
        {
            FileChooser fc ("New Edit", File::getSpecialLocation (File::userDocumentsDirectory), "*.tracktionedit");
            if (fc.browseForFileToSave (true))
                editFile = fc.getResult();
            else
                return;
        }
        
        selectionManager.deselectAll();
        editComponent = nullptr;
        
        auto projectFiles = editFile.getParentDirectory().findChildFiles(juce::File::findFiles, false, "*.tracktion");
        auto projectFile = projectFiles.data();
        auto xml = juce::parseXML (editFile);
        auto projectID_str = xml.get() ? xml->getStringAttribute("projectID") : "";
        if (projectFile && projectID_str != "")
        {
            // load edit using "tracktionedit" AND "tracktion" file
            engine.getProjectManager().addProjectToList(*projectFile, true, engine.getProjectManager().getActiveProjectsFolder());
            auto projectID = ProjectItemID{projectID_str};
            auto editState = te::loadEditFromProjectManager(engine.getProjectManager(), projectID);
            Edit::Options options =
            {
                engine,
                editState,
                projectID,
                Edit::forEditing,
                nullptr,
                Edit::getDefaultNumUndoLevels(),
                [this] { return editFile; },
                {}
            };
            edit = std::make_unique<Edit> (options);
        }
        else if (editFile.existsAsFile())
            // load edit using "tracktionedit" file only
            edit = te::loadEditFromFile (engine, editFile);
        else
            edit = te::createEmptyEdit (engine, editFile);
        
        edit->playInStopEnabled = true;
        
        auto& transport = edit->getTransport();
        transport.addChangeListener (this);
        
        editNameLabel.setText (editFile.getFileNameWithoutExtension(), dontSendNotification);
        showEditButton.onClick = [this]
        {
            te::EditFileOperations (*edit).save (true, true, false);
            editFile.revealToUser();
        };
        
        createTracksAndAssignInputs();
        
        //te::EditFileOperations (*edit).save (true, true, false);
        
        editComponent = std::make_unique<EditComponent> (*edit, selectionManager);
        addAndMakeVisible (*editComponent);
        resized();
    }
    
    void createTracksAndAssignInputs()
    {
        auto& dm = engine.getDeviceManager();

        for (int i = 0; i < dm.getNumWaveInDevices(); i++)
            if (auto wip = dm.getWaveInDevice (i))
                wip->setStereoPair (false);
        
        for (int i = 0; i < dm.getNumWaveInDevices(); i++)
        {
            if (auto wip = dm.getWaveInDevice (i))
            {
                wip->setEndToEnd (true);
                wip->setEnabled (true);
            }
        }
        
        edit->getTransport().ensureContextAllocated();
        
        int trackNum = 0;
        for (auto instance : edit->getAllInputDevices())
        {
            if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
            {
                if (auto t = EngineHelpers::getOrInsertAudioTrackAt (*edit, trackNum))
                {
                    instance->setTargetTrack (*t, 0, true, &edit->getUndoManager());
                    instance->setRecordingEnabled (*t, true);
                    
                    trackNum++;
                }
            }
        }
        
        edit->restartPlayback();
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (edit != nullptr && source == &edit->getTransport())
        {
            updatePlayButtonText();
            updateRecordButtonText();
        }
        else if (source == &selectionManager)
        {
            auto sel = selectionManager.getSelectedObject (0);
            deleteButton.setEnabled (dynamic_cast<te::Clip*> (sel) != nullptr || dynamic_cast<te::Track*> (sel) != nullptr);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordingDemo)
};

//==============================================================================
static DemoTypeBase<RecordingDemo> recordingDemo ("Audio Recording");
