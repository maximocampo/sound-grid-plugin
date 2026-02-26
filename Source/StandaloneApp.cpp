#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace juce
{

class SoundGridStandaloneApp final : public JUCEApplication
{
public:
    SoundGridStandaloneApp()
    {
        PropertiesFile::Options options;
        options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName          = "";
        appProperties.setStorageParameters (options);
    }

    const String getApplicationName() override              { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override           { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override              { return true; }
    void anotherInstanceStarted (const String&) override    {}

    StandaloneFilterWindow* createWindow()
    {
        if (Desktop::getInstance().getDisplays().displays.isEmpty())
            return nullptr;

        return new StandaloneFilterWindow (getApplicationName(),
                                           LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                                           createPluginHolder());
    }

    std::unique_ptr<StandalonePluginHolder> createPluginHolder()
    {
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig;

        return std::make_unique<StandalonePluginHolder> (appProperties.getUserSettings(),
                                                         false,
                                                         String{},
                                                         nullptr,
                                                         channelConfig,
                                                         false);
    }

    void initialise (const String&) override
    {
        mainWindow = rawToUniquePtr (createWindow());

        if (mainWindow != nullptr)
        {
            mainWindow->setUsingNativeTitleBar (true);
            mainWindow->setVisible (true);
        }
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            Timer::callAfterDelay (100, []()
            {
                if (auto app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

protected:
    ApplicationProperties appProperties;
    std::unique_ptr<StandaloneFilterWindow> mainWindow;
};

} // namespace juce

START_JUCE_APPLICATION (juce::SoundGridStandaloneApp)
