void init()
{
  // timer/counter
  TCCR0A = 1 << WGM01;  // set Clear Timer on Compare Match mode (CTC)
  TCCR0B = (0 << CS02) | (1 << CS01) | (0 << CS00);   // set clock prescaling 8
  //OCR0A = 124;      // compare match on 126.
  OCR0A = 89;      // compare match on 126.

  // Configure ADC
  ADCSRA = (1<<ADPS2) | // prescale 128
         (1<<ADPS1) |
         (0<<ADPS0); 

    ADCSRB = (1<<ADTS0) |   // Trigger source timer/counter 0
                            // Compare Match A
             (1<<ADTS1);
    ADMUX = (1 << REFS0) | // Set AVcc as reference
        (1 << ADLAR);  // ADC Left Adjust Result


}

void start()
{
  //Enable ADC
  ADCSRA |= (1 << ADATE) | // Auto Trigger enable.
        (1 << ADIE) |  // Interrupt enable.
        (1 << ADEN);   // Enable ADC.
  sei();
}

struct Sample
{
  unsigned short newData = false;
  unsigned short data = 0;
};

volatile struct Sample sample;

ISR(ADC_vect)
{
  cli();
  sample.data = (unsigned short) ADCH;
  sample.newData = true;

  TIFR0 = 0x07;
  sei();
}


// the setup function runs once when you press reset or power the board
void setup() {

  pinMode(13, OUTPUT);
  
  // initialize serial communication at 9600 bits per second:
  Serial.begin(112500);
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and other 32u4 based boards.
  }


  init();
  start();

}

void loop()
{
  for (;;)
  {
    if(sample.newData == true)
    {
      char d = sample.data;
      Serial.print(d);
    }
  }
}
