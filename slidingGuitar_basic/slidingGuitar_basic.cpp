#include "daisy_seed.h"
#include "daisysp.h"
#include "usbh_midi.h"

using namespace daisy;
using namespace daisysp;

/** Global Hardware access */
DaisySeed       hw;
MidiUsbHandler midi;
USBHostHandle usbHost;

/** FIFO to hold messages as we're ready to print them */
FIFO<MidiEvent, 128> event_log;

void USBH_Connect(void* data)
{
    hw.PrintLine("device connected");
}

void USBH_Disconnect(void* data)
{
    hw.PrintLine("device disconnected");
}

void USBH_ClassActive(void* data)
{
    if(usbHost.IsActiveClass(USBH_MIDI_CLASS))
    {
        hw.PrintLine("MIDI device class active");
        MidiUsbHandler::Config midi_config;
        midi_config.transport_config.periph = MidiUsbTransport::Config::Periph::HOST;
        midi.Init(midi_config);
        midi.StartReceive();
    }
}

void USBH_Error(void* data)
{
    hw.PrintLine("USB device error");
}
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) // Basic audio handler - bob
{
	for (size_t i = 0; i < size; i++)
	{
		out[0][i] = in[0][i]; //this is just a simple audio passthru
		out[1][i] = in[1][i];
	}
}

int main(void) {
	    /** Initialize our hardware */
    hw.Init();

    hw.StartLog(true);

    hw.PrintLine("MIDI USB Host start");

    /** Configure USB host */
    USBHostHandle::Config usbhConfig;
    usbhConfig.connect_callback = USBH_Connect,
    usbhConfig.disconnect_callback = USBH_Disconnect,
    usbhConfig.class_active_callback = USBH_ClassActive,
    usbhConfig.error_callback = USBH_Error,
    usbHost.Init(usbhConfig);

    usbHost.RegisterClass(USBH_MIDI_CLASS);

    uint32_t now      = System::GetNow();
    uint32_t log_time = System::GetNow();
    uint32_t blink_time = 0;
    bool ledState = false;

    hw.PrintLine("MIDI USB Host initialized");

	hw.SetAudioBlockSize(64); // number of samples handled per callback - 64 is standard - bob
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	hw.StartAudio(AudioCallback);
	while(1) {
		now = System::GetNow();

        if (now > blink_time) {
            hw.SetLed(ledState);
            ledState = !ledState;
            if (usbHost.GetPresent())
                blink_time = now + 400;
            else
                blink_time = now + 80;
        }
        /** Run USB host process */
        usbHost.Process();

        if(usbHost.IsActiveClass(USBH_MIDI_CLASS) && midi.RxActive()) {
            /** Process MIDI in the background */
            midi.Listen();

            /** Loop through any MIDI Events */
            while(midi.HasEvents()) {
                MidiEvent msg = midi.PopEvent();

                /** Handle messages as they come in 
                 *  See DaisyExamples for some examples of this
                 */
                switch(msg.type)
                {
                    case NoteOn:
                        // Do something on Note On events
                        {
                            uint8_t bytes[3] = {0x90, 0x00, 0x00};
                            bytes[1] = msg.data[0];
                            bytes[2] = msg.data[1];
                            midi.SendMessage(bytes, 3);
                        }
                        break;
                    default: break;
                }

                /** Regardless of message, let's add the message data to our queue to output */
                event_log.PushBack(msg);
            }

            /** Now separately, every 5ms we'll print the top message in our queue if there is one */
            if(now - log_time > 5)
            {
                log_time = now;
                if(!event_log.IsEmpty())
                {
                    auto msg = event_log.PopFront();
                    char outstr[128];
                    const char* type_str = MidiEvent::GetTypeAsString(msg);
                    sprintf(outstr,
                            "time:\t%ld\ttype: %s\tChannel:  %d\tData MSB: "
                            "%d\tData LSB: %d\n",
                            now,
                            type_str,
                            msg.channel,
                            msg.data[0],
                            msg.data[1]);
                    hw.PrintLine(outstr);
                }
            }
        }
	}
}
