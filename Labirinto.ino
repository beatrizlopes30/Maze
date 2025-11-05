#include <Wire.h>
#include <VL53L0X.h>

// Definições dos pinos
#define TRIG_PIN 22
#define ECHO_PIN 23
#define MOTOR_A1 2
#define MOTOR_A2 3
#define MOTOR_B1 4
#define MOTOR_B2 5
#define MOTOR_ENA 6
#define MOTOR_ENB 7

// Sensores IR (apenas 2 laterais)
#define IR_LADO_DIREITO A0     // IR lateral direito
#define IR_LADO_ESQUERDO A1    // IR lateral esquerdo

// Pinoss para controle dos sensores laser (XSHUT)
#define XSHUT_LASER_DIREITA 8
#define XSHUT_LASER_ESQUERDA 9

// Distâncias de segurança (em mm)
#define DISTANCIA_SEGURO 400   // 40cm
#define DISTANCIA_ALERTA 250   // 25cm  
#define DISTANCIA_VIRAR 150    // 15cm
#define DISTANCIA_EMERGENCIA 80 // 8cm
#define LIMIAR_IR 400          // Valor para detectar parede

// Objetos para os sensores laser laterais
VL53L0X sensorLaserDireita;
VL53L0X sensorLaserEsquerda;

// Variáveis
unsigned int distanciaFrente = 0;
unsigned int distanciaDireita = 0;
unsigned int distanciaEsquerda = 0;
int irLadoDir = 0, irLadoEsq = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  // Inicializar sensores laser laterais
  inicializarSensoresLaser();
  
  // Configurar pinos
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  pinMode(MOTOR_A1, OUTPUT);
  pinMode(MOTOR_A2, OUTPUT);
  pinMode(MOTOR_B1, OUTPUT);
  pinMode(MOTOR_B2, OUTPUT);
  
  Serial.println("Sistema inicializado!");
  delay(1000);
}

void inicializarSensoresLaser() {

  pinMode(XSHUT_LASER_DIREITA, OUTPUT);
  pinMode(XSHUT_LASER_ESQUERDA, OUTPUT);
  
  digitalWrite(XSHUT_LASER_DIREITA, LOW);
  digitalWrite(XSHUT_LASER_ESQUERDA, LOW);
  delay(10);
  
  // Ativar e mudar endereço sensor lasers
  digitalWrite(XSHUT_LASER_DIREITA, HIGH);
  delay(10);
  if (!sensorLaserDireita.init()) {
    Serial.println("Falha sensor laser direita!");
    while(1);
  }
  sensorLaserDireita.setAddress(0x31);
  sensorLaserDireita.setTimeout(500);
  
  digitalWrite(XSHUT_LASER_ESQUERDA, HIGH);
  delay(10);
  if (!sensorLaserEsquerda.init()) {
    Serial.println("Falha sensor laser esquerda!");
    while(1);
  }
  sensorLaserEsquerda.setAddress(0x32);
  sensorLaserEsquerda.setTimeout(500);
  
  Serial.println("Sensores laser laterais inicializados!");
}

void loop() {
  // Ler todos os sensores
  distanciaFrente = lerSensorUltrassom();
  distanciaDireita = lerSensorLaserDireita();
  distanciaEsquerda = lerSensorLaserEsquerda();
  lerSensoresIR();
  
  // Exibir leituras
  Serial.print("Frente: ");
  Serial.print(distanciaFrente);
  Serial.print("mm | Laser D: ");
  Serial.print(distanciaDireita);
  Serial.print("mm | Laser E: ");
  Serial.print(distanciaEsquerda);
  Serial.print("mm | IR D: ");
  Serial.print(irLadoDir);
  Serial.print(" | IR E: ");
  Serial.println(irLadoEsq);
  
  // Tomar decisão
  tomarDecisaoInteligente();
  
  delay(100);
}

void lerSensoresIR() {
  irLadoDir = analogRead(IR_LADO_DIREITO);
  irLadoEsq = analogRead(IR_LADO_ESQUERDO);
}

bool paredeProximaIR(int valorIR) {
  return valorIR > LIMIAR_IR;
}

unsigned int lerSensorLaserDireita() {
  unsigned int dist = sensorLaserDireita.readRangeSingleMillimeters();
  if (sensorLaserDireita.timeoutOccurred()) return 9999;
  return dist;
}

unsigned int lerSensorLaserEsquerda() {
  unsigned int dist = sensorLaserEsquerda.readRangeSingleMillimeters();
  if (sensorLaserEsquerda.timeoutOccurred()) return 9999;
  return dist;
}

unsigned int lerSensorUltrassom() {
  // Faz 3 medições e retorna a mediana
  unsigned int leituras[3];
  for(int i = 0; i < 3; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duracao = pulseIn(ECHO_PIN, HIGH, 30000);
    leituras[i] = (duracao == 0) ? 9999 : (duracao * 0.34) / 2;
    delay(5);
  }
  
  // Ordena e retorna mediana
  if(leituras[0] > leituras[1]) swap(leituras[0], leituras[1]);
  if(leituras[1] > leituras[2]) swap(leituras[1], leituras[2]);
  if(leituras[0] > leituras[1]) swap(leituras[0], leituras[1]);
  
  return leituras[1];
}

void tomarDecisaoInteligente() {
  bool paredeLadoDir = paredeProximaIR(irLadoDir);
  bool paredeLadoEsq = paredeProximaIR(irLadoEsq);
  
  // Lógica  de navegação
  if (distanciaFrente <= DISTANCIA_EMERGENCIA) {
    // EMERGÊNCIA - muito próximo na frente
    Serial.println(">>> RECUAR EMERGÊNCIA");
    recuar(150);
    delay(200);
    
    // Decidir para onde virar baseado nas leituras laterais
    if (distanciaDireita > distanciaEsquerda) {
      Serial.println(">>> VIRAR DIREITA APÓS RECUAR");
      virarDireita(180);
      delay(400);
    } else {
      Serial.println(">>> VIRAR ESQUERDA APÓS RECUAR");
      virarEsquerda(180);
      delay(400);
    }
  }
  else if (distanciaFrente <= DISTANCIA_VIRAR) {
    // Hora de virar - usar informações laterais dos lasers
    parar();
    delay(200);
    
    if (deveVirarDireita(distanciaDireita, distanciaEsquerda, paredeLadoDir, paredeLadoEsq)) {
      Serial.println(">>> VIRAR DIREITA ESTRATÉGICA");
      virarDireita(180);
      delay(350);
    } else {
      Serial.println(">>> VIRAR ESQUERDA ESTRATÉGICA");
      virarEsquerda(180);
      delay(350);
    }
  }
  else if (distanciaFrente <= DISTANCIA_ALERTA) {
    // Aproximando de obstáculo - navegação preventiva
    navegacaoPreventiva(distanciaDireita, distanciaEsquerda, paredeLadoDir, paredeLadoEsq);
  }
  else {
    // Caminho livre - navegação normal
    navegacaoNormal(distanciaDireita, distanciaEsquerda, paredeLadoDir, paredeLadoEsq);
  }
}

bool deveVirarDireita(unsigned int distDir, unsigned int distEsq, bool paredeDir, bool paredeEsq) {
  // virar para o lado com mais espaço
  
  // Se um lado está claramente mais livre
  if (distDir > distEsq + 50) { // Pelo menos 5 cm mais livre
    return true;
  }
  else if (distEsq > distDir + 50) {
    return false;
  }
  
  // Se as distâncias são similares, usar algoritmo wall follower direito
  if (!paredeDir) {
    return true;
  }
  else if (!paredeEsq) {
    return false;
  }
  
  // Padrão: direita
  return true;
}

void navegacaoPreventiva(unsigned int distDir, unsigned int distEsq, bool paredeDir, bool paredeEsq) {
  // Navegação quando estamos nos aproximando de um obstáculo frontal
  
  if (paredeEsq && !paredeDir && distDir > 20) {
    Serial.println(">>> CURVA SUAVE DIREITA (PREVENTIVA)");
    moverAjustado(120, 80);
  }
  else if (paredeDir && !paredeEsq && distEsq > 200) {
    Serial.println(">>> CURVA SUAVE ESQUERDA (PREVENTIVA)");
    moverAjustado(80, 120);
  }
  else if (distDir > distEsq && distDir > 150) {
    Serial.println(">>> TENDÊNCIA DIREITA (LASER)");
    moverAjustado(100, 80);
  }
  else if (distEsq > distDir && distEsq > 150) {
    Serial.println(">>> TENDÊNCIA ESQUERDA (LASER)");
    moverAjustado(80, 100);
  }
  else {
    Serial.println(">>> FRENTE CAUTELOSA");
    moverFrente(80);
  }
}

void navegacaoNormal(unsigned int distDir, unsigned int distEsq, bool paredeDir, bool paredeEsq) {
  // Navegação quando o caminho frontal está livre
  
  if (paredeEsq && paredeDir) {
    // Corredor - seguir reto centralizado
    Serial.println(">>> CORREDOR - CENTRALIZADO");
    
    // Usar lasers para manter centralizado
    int diferenca = distanciaEsquerda - distanciaDireita;
    if (abs(diferenca) > 50) { // Se diferença maior que 5cm
      if (diferenca > 0) {
        moverAjustado(140, 160); // Ajustar para esquerda
      } else {
        moverAjustado(160, 140); // Ajustar para direita
      }
    } else {
      moverFrente(150);
    }
  }
  else if (paredeEsq && !paredeDir) {
    // Parede apenas na esquerda - seguir próximo à parede
    Serial.println(">>> SEGUIR COM PAREDE ESQUERDA");
    
    if (distanciaEsquerda < 100) { // Muito perto da parede
      moverAjustado(120, 150); // Afastar suavemente
    } else if (distanciaEsquerda > 200) { // Muito longe da parede
      moverAjustado(150, 120); // Aproximar suavemente
    } else {
      moverFrente(150); // Distância ideal
    }
  }
  else if (paredeDir && !paredeEsq) {
    // Parede apenas na direita - seguir próximo à parede
    Serial.println(">>> SEGUIR COM PAREDE DIREITA");
    
    if (distanciaDireita < 100) { // Muito perto da parede
      moverAjustado(150, 120); // Afastar suavemente
    } else if (distanciaDireita > 200) { // Muito longe da parede
      moverAjustado(120, 150); // Aproximar suavemente
    } else {
      moverFrente(150); // Distância ideal
    }
  }
  else {
    // Caminho totalmente livre
    Serial.println(">>> SEGUIR FRENTE LIVRE");
    moverFrente(180);
  }
}

// Funções de movimento
void moverFrente(int velocidade) {
  digitalWrite(MOTOR_A1, HIGH);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, HIGH);
  digitalWrite(MOTOR_B2, LOW);
  analogWrite(MOTOR_ENA, velocidade);
  analogWrite(MOTOR_ENB, velocidade);
}

void moverAjustado(int velEsquerda, int velDireita) {
  digitalWrite(MOTOR_A1, HIGH);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, HIGH);
  digitalWrite(MOTOR_B2, LOW);
  analogWrite(MOTOR_ENA, velEsquerda);
  analogWrite(MOTOR_ENB, velDireita);
}

void virarDireita(int velocidade) {
  digitalWrite(MOTOR_A1, HIGH);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, HIGH);
  analogWrite(MOTOR_ENA, velocidade);
  analogWrite(MOTOR_ENB, velocidade);
}

void virarEsquerda(int velocidade) {
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, HIGH);
  digitalWrite(MOTOR_B1, HIGH);
  digitalWrite(MOTOR_B2, LOW);
  analogWrite(MOTOR_ENA, velocidade);
  analogWrite(MOTOR_ENB, velocidade);
}

void recuar(int velocidade) {
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, HIGH);
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, HIGH);
  analogWrite(MOTOR_ENA, velocidade);
  analogWrite(MOTOR_ENB, velocidade);
}

void parar() {
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, LOW);
  analogWrite(MOTOR_ENA, 0);
  analogWrite(MOTOR_ENB, 0);
}