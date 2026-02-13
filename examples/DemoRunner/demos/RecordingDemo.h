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
    
        //TODO alert if position % 1000 is > 1
        importBPMsButton.onClick = [this] {
            FileChooser fc ("Import Midi tempo", File::getSpecialLocation (File::userDocumentsDirectory), "*.mid");
            if (fc.browseForFileToOpen())
            {
                // find Movements track
                Track* movementsTrack = nullptr;
                for (auto track : tracktion::getClipTracks(*edit))
                    if (track->getName() == "MIDI Tempo Time Sign.")
                        movementsTrack = track;
                
                if (movementsTrack == nullptr)
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "", "Could not find metronome track");
                    return;
                }
                
                int movementsTrack_index = movementsTrack->getIndexInEditTrackList();
                tracktion::Clipboard::pasteMIDIFileIntoEdit(*edit, fc.getResult(), movementsTrack_index, edit->getTransport().getPosition(), true, false);
            }
        };
        
        importMetronomeButton.onClick = [this] {
            FileChooser fc ("Import Midi metronome", File::getSpecialLocation (File::userDocumentsDirectory), "*.mid");
            if (fc.browseForFileToOpen())
            {
                // find metronomeTrack
                Track* metronomeTrack = nullptr;
                for (auto track : tracktion::getClipTracks(*edit))
                    if (track->getName() == "Metronome")
                        metronomeTrack = track;
                
                if (metronomeTrack == nullptr)
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "", "Could not find metronome track");
                    return;
                }
                int metronomeTrack_index = metronomeTrack->getIndexInEditTrackList();
                tracktion::Clipboard::pasteMIDIFileIntoEdit(*edit, fc.getResult(), metronomeTrack_index, edit->getTransport().getPosition(), true, true, false);
            }
        };
        
        importFLACsButton.onClick = [this] {
            FileChooser fc ("Import FLACs", editFile, "*.flac");
            if (fc.browseForMultipleFilesToOpen())
            {
                // sort files by name, so they're imported in the correct musical order
                auto results = fc.getResults();
                auto comparator = juce::File::NaturalFileComparator(true);
                results.sort(comparator);
                
                for (auto& file : results)
                {
                    // copy file to Imported folder, if necessary
                    auto fileCopy = editFile.getParentDirectory().getChildFile("Imported").getChildFile(file.getFileName());
                    file.copyFileTo(fileCopy);
                    file = fileCopy;
                    jassert(file.existsAsFile());
                    
                    auto fileName              = file.getFileNameWithoutExtension();
                    //                           Ex: "Mvt-01_044bpm-01-KB-ORG-Organo MD"
                         
                    auto movement              = fileName.substring(4, 6).getIntValue();
                    auto bpm                   = fileName.substring(7, 10).getIntValue();
                    auto partOrder             = fileName.substring(14, 16);
                    auto familyShort           = fileName.substring(17, 19);
                    auto instrumentShort       = fileName.substring(20, 23);
                    auto partName              = fileName.substring(24);
                    
                    if (fileName.substring(0,   4) != "Mvt-"  ||
                        fileName.substring(10, 14) != "bpm-"  ||
                        !juce::Range(1, 99).contains(movement)||
                        !juce::Range(10, 360).contains(bpm)   ||
                        !juce::Range(01, 99).contains(partOrder.getIntValue()))
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "", 
                                                                "Invalid file name: \n" + file.getFileName() + "\n\n" +
                                                                "Valid file name is for example: \n" +
                                                                "Mvt-01_139bpm-01-WW-FLT-Flauto I.flac");
                        return;
                    }
                    
                    
                    auto family = families.find(familyShort);
                    if (family == families.end()){
                        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "",
                                                                "Invalid family for: \n" + file.getFileName());
                        return;
                    }
                    
                    // TODO assert that all flac files have same length as midi file for this movement
                    
                    // find parent folder track
                    tracktion::FolderTrack* parentTrack = nullptr;
                    for (auto track : tracktion::getTracksOfType<tracktion::FolderTrack> (*edit, true)){
                        if (track->getName() == family->second){
                            parentTrack = track;
                            break;
                        }
                    }
                    if (parentTrack == nullptr){
                        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "",
                                                                "Edit doesn't contain folder track \t" + family->second);
                        return;
                    }
                    
                    tracktion::ClipTrack* clipTrack = nullptr;
                    for (auto track : tracktion::getAudioTracks(*edit)){
                        auto substr = track->getName().substring(defaultPosition.length());
                        if (track->getName().substring(defaultPosition.length()) == partName &&
                            track->getParentFolderTrack()->getName() == family->second){
                            // found existing track in the same family
                            clipTrack = track;
                            break;
                        }
                    }
                    if (clipTrack == nullptr)
                    {
                        // create new track
                        auto lastSiblingTrack =   parentTrack->getSubTrackList() == nullptr ? nullptr
                                                : parentTrack->getSubTrackList()->objects.isEmpty() ? nullptr
                                                : parentTrack->getSubTrackList()->objects.getLast();
                        clipTrack = edit->insertNewAudioTrack(TrackInsertPoint(parentTrack, lastSiblingTrack), nullptr).get();
                        clipTrack->setName(defaultPosition + partName);
                    }
                    
                    // TODO assert all FLACs with same bpm have exactly the same length
                    
                    // insert wave clip on clip track
                    te::AudioFile audioFile{ engine, file };
                    auto start = edit->getTransport().getPosition();
                    using namespace std::chrono_literals;
                    int numerator   = edit->tempoSequence.getTimeSigAt(start).numerator.get();
                    int denominator = edit->tempoSequence.getTimeSigAt(start).denominator.get();
                    
                    // maybe later to fix Waveform's "Remove silence" bug when denominator != 4
                    //denominator = 4;
                    
                    auto end = start + te::TimeDuration::fromSeconds (audioFile.getLength());
                    auto clip = clipTrack->insertWaveClip (fileName, file,  { { start, end }, {} }, false);
                    clip->setUsesProxy(false);
                    clip->setAutoTempo(true);
                    clip->setAutoPitch(true);
                    clip->setMuted(false);
                    clip->setLength(clip->getMaximumLength(), true);
                    
                    clip->getLoopInfo().setNumerator(numerator);
                    clip->getLoopInfo().setDenominator(denominator);
                    clip->getLoopInfo().setBpm(bpm, te::AudioFileInfo::parse (clip->getAudioFile()));
                    clip->getLoopInfo().setRootNote(60); // all clips have C as root note, even if current key is different
                }
            }
        };
        
        reloadButton.onClick = [this] {
            createOrLoadEdit (editFile);
        };
        
        saveButton.onClick = [this] 
        {
            // copy <TEMPO_SEQUENCE> into <TEMPOSEQUENCE_ORCHESTRATOR>
            auto origTempos = edit->state.getOrCreateChildWithName ("TEMPOSEQUENCE_ORCHESTRATOR", nullptr);
            origTempos.removeAllChildren(nullptr);
            origTempos.copyPropertiesAndChildrenFrom(edit->tempoSequence.getState(), nullptr);
            
            // copy <TEMPO_SEQUENCE> into <TEMPOSEQUENCE_USER_100_PERCENT>
            auto userTempos_100PCent = edit->state.getOrCreateChildWithName ("TEMPOSEQUENCE_USER_100_PERCENT", nullptr);
            userTempos_100PCent.removeAllChildren(nullptr);
            userTempos_100PCent.copyPropertiesAndChildrenFrom(edit->tempoSequence.getState(), nullptr);
            
            // copy <TEMPO_SEQUENCE> into <TEMPOSEQUENCE_USER_100_PERCENT>
            auto userTempos_xxxPCent = edit->state.getOrCreateChildWithName ("TEMPOSEQUENCE_USER_xxx_PERCENT", nullptr);
            userTempos_xxxPCent.removeAllChildren(nullptr);
            userTempos_xxxPCent.copyPropertiesAndChildrenFrom(edit->tempoSequence.getState(), nullptr);
            
            te::EditFileOperations (*edit).save (true, true, false);
        };
        
        updatePlayButtonText();
        updateRecordButtonText();
        editNameLabel.setJustificationType (Justification::centred);
        Helpers::addAndMakeVisible (*this, { &loadEditButton, &newEditButton, &playPauseButton, &recordButton, &showEditButton,
                                             &newTrackButton, &clearTracksButton, &deleteButton, &editNameLabel,
                                             &undoButton, &redoButton, &importBPMsButton, &importMetronomeButton, &audioSettingsButton,
                                             &reloadButton, &importFLACsButton, &exportFLACsButton, &saveButton
        });

        deleteButton.setEnabled (false);
        
        auto d = File::getSpecialLocation (File::tempDirectory).getChildFile ("RecordingDemo");
        d.createDirectory();
        
//        if (editFile.existsAsFile())
//            createOrLoadEdit (editFile);
//        else
//            createOrLoadEdit (d.getNonexistentChildFile ("Test", ".tracktionedit", false));
        
        selectionManager.addChangeListener (this);
        
        setupButtons();
        
        setSize (700, 500);
    }

    ~RecordingDemo() override
    {
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
        auto topR = r.removeFromTop (30);
        loadEditButton.setBounds (topR.removeFromLeft (100).reduced (2));
        saveButton.setBounds (topR.removeFromLeft (100).reduced (2));
        playPauseButton.setBounds (topR.removeFromLeft (60).reduced (2));
        importBPMsButton.setBounds(topR.removeFromLeft(200).reduced(2));
        importMetronomeButton.setBounds(topR.removeFromLeft(220).reduced(2));
        importFLACsButton.setBounds(topR.removeFromLeft(200).reduced(2));
        audioSettingsButton.setBounds(topR.removeFromRight(120).reduced(2));

        if (editComponent != nullptr)
            editComponent->setBounds (r);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<EditComponent> editComponent;
    juce::File editFile {"/Users/mickael/Library/Synchestra/Pieces/Ravel - Bolero/Import Tempo changes.tracktionedit"};

    TextButton  loadEditButton { "Load edit" }, newEditButton { "New" }, playPauseButton { "Play" }, recordButton { "Record" },
                showEditButton { "Show Edit" }, newTrackButton { "New Track" }, clearTracksButton { "Clear Tracks" }, deleteButton { "Delete" },
                undoButton {"Undo"}, redoButton {"Redo"}, importBPMsButton {"Import MIDI tempo Mvt-xx"}, reloadButton {"Reload Edit"}, saveButton {"Save Edit"},
                importMetronomeButton {"Import MIDI Metronome Mvt-xx"},
                importFLACsButton {"Import FLACs Mvt-xx"}, exportFLACsButton {"Export FLACs"}, audioSettingsButton {"Audio settings"};
    Label editNameLabel { "No Edit Loaded" };
    
    
    std::map<String, String> families = {
        {"WW", "Woodwinds"},
        {"BR", "Brass"},
        {"ST", "Strings"},
        {"S2", "Strings 2"},
        {"KB", "Keyboards"},
        {"PL", "Plucked"},
        {"PC", "Percussions"},
        {"FR", "Fretted"},
        {"EL", "Electronic"},
        {"CH", "Choir"},
        {"VO", "Vocals"}};
    
    juce::String defaultPosition {juce::CharPointer_UTF8 ("Sitting: 090\xc2\xb0.4m ")};

    //==============================================================================
    void setupButtons()
    {
        
        loadEditButton.onClick  = [this] {
            auto fc = std::make_shared<FileChooser> ("Please select an edit file to load...",
                                                     getApplicationSettings()->getValue("lastDirectory"),
                                                     "*.tracktionedit");

            fc->launchAsync (FileBrowserComponent::openMode + FileBrowserComponent::canSelectFiles,
                             [this, fc] (const FileChooser&)
                             {
                                const auto f = fc->getResult();

                                if (f.existsAsFile()){
                                    editFile = f;
                                    createOrLoadEdit (editFile);
                                    getApplicationSettings()->setValue("lastDirectory", f.getParentDirectory().getFullPathName());
                                }
                             });
        };
        
        playPauseButton.onClick = [this]
        {
            //bool wasRecording = edit->getTransport().isRecording();
            EngineHelpers::togglePlay (*edit);
        };
        recordButton.onClick = [this]
        {
            //bool wasRecording = edit->getTransport().isRecording();
            EngineHelpers::toggleRecord (*edit);
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
        undoButton.onClick = [this]
        {
            edit->getUndoManager().undo();
        };
        redoButton.onClick = [this]
        {
            edit->getUndoManager().redo();
        };
        
        audioSettingsButton.onClick = [this] { EngineHelpers::showAudioDeviceSettings (engine); };
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
        
        //createTracksAndAssignInputs();
        
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
