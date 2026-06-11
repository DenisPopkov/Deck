#include <juce_gui_extra/juce_gui_extra.h>
#include "ui/MainComponent.h"
#include "ui/look/CassetteLook.h"
#include "util/AppLog.h"

class CassetteAutoMasterApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "CassetteMaster"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        cassette::initLogging();
        lookAndFeel = std::make_unique<cassette::CassetteLook>();
        juce::Desktop::getInstance().setDefaultLookAndFeel(lookAndFeel.get());
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::Desktop::getInstance().setDefaultLookAndFeel(nullptr);
        lookAndFeel = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(
                  std::move(name),
                  juce::Desktop::getInstance().getDefaultLookAndFeel()
                      .findColour(juce::ResizableWindow::backgroundColourId),
                  DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new cassette::MainComponent(), true);

            setResizable(true, true);
            setResizeLimits(960, 640, 16384, 16384);

            centreWithSize(1100, 820);
            setVisible(true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (toggleFullScreenFromKey(key))
                return true;

            return DocumentWindow::keyPressed(key);
        }

    private:
        bool toggleFullScreenFromKey(const juce::KeyPress& key)
        {
            const auto mods = key.getModifiers();

#if JUCE_MAC
            if (mods.isCommandDown() && mods.isCtrlDown() && key.getTextCharacter() == 'f')
            {
                setFullScreen(!isFullScreen());
                return true;
            }
#endif

            if (key.getKeyCode() == juce::KeyPress::F11Key)
            {
                setFullScreen(!isFullScreen());
                return true;
            }

            return false;
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<cassette::CassetteLook> lookAndFeel;
};

START_JUCE_APPLICATION(CassetteAutoMasterApplication)
