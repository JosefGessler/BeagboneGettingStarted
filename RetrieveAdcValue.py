import time

def read_adc_value():
    try:
        with open("/dev/ain4_input", "r") as f:
            value_str = f.read().strip()
            value_mV = int(value_str)
            value_V = value_mV / 1000.0
            return value_V
    except Exception as e:
        print(f"Failed to read ADC value: {e}")
        return None

def main():
    while True:
        adc_value = read_adc_value()
        if adc_value is not None:
            print(f"ADC Value: {adc_value:.3f} V")
        time.sleep(1)

if __name__ == "__main__":
    main()
