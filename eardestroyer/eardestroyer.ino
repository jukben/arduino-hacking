#include <avr/sleep.h>
#include <RH_ASK.h>

bool interrupted = false;
bool armed = true;

uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
uint8_t buflen = sizeof(buf);

long wokenUpAtMillis = 0;
long interval = 5000; // 5s

int MAGNETIC_GATE = 3;
int SIREN = 4;
int RED_DIODE = 5;
int RADIO_WAKE = 6;

RH_ASK radio(1000, 2);

void setup()
{
  pinMode(MAGNETIC_GATE, INPUT_PULLUP);
  pinMode(RED_DIODE, OUTPUT);
  pinMode(SIREN, OUTPUT);
  pinMode(RADIO_WAKE, OUTPUT);

  digitalWrite(RED_DIODE, LOW);
  digitalWrite(SIREN, LOW);
  digitalWrite(RADIO_WAKE, LOW);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);

  if (!radio.init())
  {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void wake()
{
  interrupted = true;
  // cancel sleep as a precaution
  sleep_disable();
  // precautionary while we do other stuff
  detachInterrupt(digitalPinToInterrupt(MAGNETIC_GATE));
  // set currently passed miliseconds since the Arudino board began running (will overflow ater ~50 days, shoudn't be problem for us)
  wokenUpAtMillis = millis();
}

// the loop function runs over and over again forever
void loop()
{
  if (interrupted)
  {
    int openDoors = digitalRead(MAGNETIC_GATE);

    if (openDoors)
    {
      unsigned long currentMillis = millis();

      // wake up radio module to allow listening for the deactivate signal
      digitalWrite(RADIO_WAKE, HIGH);
      // light up RED LED to signalize we have to disable it before the alarm goes crazy
      digitalWrite(RED_DIODE, HIGH);

      if (currentMillis - wokenUpAtMillis > interval)
      {
        // boom
        digitalWrite(SIREN, HIGH);
      }

      if (radio.recv(buf, &buflen))
      {
        buf[buflen] = 0; // end buffer (I guess, I haven't done C in years)

        if (strcmp((char *)buf, "ed:jb:3576") == 0)
        {
          // let's go to sleep...
          interrupted = false;
          // ...but mark it as unarmed
          armed = false;
        }
      }
    }
    else if (!armed)
    {
      // let's go to sleep...
      interrupted = false;
      // and be ready to ring the alarm again next time someone open the doors
      armed = true;
    }

    // let's not sleep yet
    return;
  }

  digitalWrite(RED_DIODE, LOW);
  digitalWrite(SIREN, LOW);
  digitalWrite(RADIO_WAKE, LOW);

  // code for deep-sleep copied from http://www.gammon.com.au/power

  ADCSRA = 0;

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // Do not interrupt before we go to sleep, or the
  // ISR will detach interrupts and we won't wake.
  noInterrupts();

  // will be called when pin D2 goes low
  attachInterrupt(digitalPinToInterrupt(MAGNETIC_GATE), wake, RISING);
  EIFR = bit(INTF0); // clear flag for interrupt 0

  // turn off brown-out enable in software
  // BODS must be set to one and BODSE must be set to zero within four clock cycles
  MCUCR = bit(BODS) | bit(BODSE);
  // The BODS bit is automatically cleared after three clock cycles
  MCUCR = bit(BODS);

  // We are guaranteed that the sleep_cpu call will be done
  // as the processor executes the next instruction after
  // interrupts are turned on.
  interrupts(); // one cycle
  sleep_cpu();  // one cycle
}
