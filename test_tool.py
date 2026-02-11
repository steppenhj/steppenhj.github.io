import serial
import time
import random
import csv
import threading

# ==========================================
# [설정] 본인의 포트 번호로 바꾸세요!
PORT = 'COM3' 
BAUDRATE = 115200
TEST_DURATION = 15  # 15초 동안 테스트
# ==========================================

def run_test():
    filename = input("저장할 파일명 입력 (예: result.csv): ").strip()
    if not filename.endswith(".csv"):
        filename += ".csv"

    try:
        ser = serial.Serial(PORT, BAUDRATE, timeout=1)
        print(f"✅ {PORT} 연결 성공! 테스트를 시작합니다...")
    except Exception as e:
        print(f"❌ 포트 연결 실패: {e}")
        return

    # CSV 파일 열기
    with open(filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['Time_Diff', 'Max_Jitter', 'Min_Jitter']) # 헤더 작성

        start_time = time.time()
        
        # 1. 수신 스레드 (로그 받아서 저장)
        def read_serial():
            while time.time() - start_time < TEST_DURATION:
                if ser.in_waiting > 0:
                    try:
                        # 한 줄 읽기
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        
                        # "PERF:" 로 시작하는 데이터만 골라냄
                        if line.startswith("PERF"):
                            print(f"📥 수신: {line}") # 화면에 보여줌
                            
                            # 데이터 파싱: "PERF: 20, MAX: 21, MIN: 19" -> [20, 21, 19]
                            parts = line.replace("PERF:", "").replace("MAX:", "").replace("MIN:", "")
                            data = [x.strip() for x in parts.split(',')]
                            
                            if len(data) == 3:
                                writer.writerow(data) # 엑셀에 저장
                    except:
                        pass

        # 스레드 시작
        t = threading.Thread(target=read_serial)
        t.start()

        # 2. 송신 루프 (스트레스 주기)
        print("🚀 스트레스 테스트 중... (명령 전송)")
        while time.time() - start_time < TEST_DURATION:
            # 랜덤 명령 생성
            speed = 900
            angle = random.randint(1150, 1850)
            cmd = f"{speed},{angle}\n"
            
            # 전송
            ser.write(cmd.encode())
            
            # 0.02초 (50Hz) 간격으로 난사
            time.sleep(0.02)

        print("\n🛑 테스트 종료!")
        ser.close()
        t.join()
        print(f"💾 {filename} 저장 완료. 엑셀에서 열어보세요.")

if __name__ == "__main__":
    run_test()