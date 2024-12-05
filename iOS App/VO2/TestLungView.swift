import SwiftUI

struct LungTestView: View {
    @State private var testStarted = false
    @State private var lungCapacity: Double = 0.0 // En litres
    @State private var timerCount: Int = 0
    @State private var timer: Timer? = nil

    var body: some View {
        VStack(spacing: 30) {
            Text("Test Your Lung Capacity")
                .font(.largeTitle)
                .fontWeight(.bold)
                .foregroundColor(.blue)
            
            // Afficheur 7 segments
            SevenSegmentDisplay(value: lungCapacity)
                .frame(height: 200)
                .padding()
            
            // Bouton pour démarrer/arrêter le test
            Button(action: {
                testStarted.toggle()
                if testStarted {
                    startSimulation()
                } else {
                    stopSimulation()
                }
            }) {
                Text(testStarted ? "Stop Test" : "Start Test")
                    .font(.headline)
                    .padding()
                    .frame(width: 200)
                    .background(testStarted ? Color.red : Color.green)
                    .foregroundColor(.white)
                    .cornerRadius(10)
            }
            
            // Affichage du compteur
            if testStarted {
                Text("\(timerCount) seconds")
                    .font(.largeTitle)
                    .fontWeight(.bold)
                    .padding()
            }
        }
        .onDisappear {
            stopSimulation()
        }
    }
    
    func startSimulation() {
        lungCapacity = 0.0
        timerCount = 0

        timer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
            timerCount += 1
            lungCapacity = min(Double(timerCount) * 5.0 / 60.0, 5.0) // Capacité max de 5.0 L
            
            if timerCount >= 60 {
                stopSimulation()
            }
        }
    }
    
    func stopSimulation() {
        timer?.invalidate()
        timer = nil
        testStarted = false
    }
}

struct SevenSegmentDisplay: View {
    let value: Double
    
    var body: some View {
        VStack(spacing: 10) {
            HStack(spacing: 5) {
                let digits = digits(for: value)
                
                // Assure-toi que tu passes correctement les trois digits séparés
                if digits.count == 3 {
                    DigitView(digit1: digits[0], digit2: digits[1], digit3: digits[2])
                }
            }
            .padding()
            
            Text("Liters")
                .font(.title)
                .foregroundColor(.green)
        }
    }
    
    private func digits(for value: Double) -> [Int] {
        // On extrait 3 digits pour afficher la valeur
        let roundedValue = Int(value * 100) // Multiplie pour extraire les chiffres après la virgule
        let firstDigit = roundedValue / 100 % 10
        let secondDigit = roundedValue / 10 % 10
        let thirdDigit = roundedValue % 10
        return [firstDigit, secondDigit, thirdDigit]
    }
}

struct DigitView: View {
    let digit1: Int
    let digit2: Int
    let digit3: Int
    
    // Carte des segments à afficher pour chaque chiffre
    private let segments = [
        [true, true, true, true, true, true, false],   // 0
        [false, true, true, false, false, false, false], // 1
        [true, true, false, true, true, false, true],  // 2
        [true, true, true, true, false, false, true],  // 3
        [false, true, true, false, false, true, true], // 4
        [true, false, true, true, false, true, true],  // 5
        [true, false, true, true, true, true, true],   // 6
        [true, true, true, false, false, false, false], // 7
        [true, true, true, true, true, true, true],    // 8
        [true, true, true, false, false, true, true]   // 9
    ]
    
    var body: some View {
        HStack(spacing: 10) { // Ajuste l'espacement global entre les digits et les séparateurs
            // Premier digit
            ZStack {
                Color.black
                    .frame(width: 60, height: 120)
                    .cornerRadius(10)
                
                let activeSegments = segments[digit1]
                
                ForEach(0..<7) { index in
                    SegmentView(isActive: activeSegments[index], index: index)
                }
            }
            
            // Virgule
            Circle()
                .fill(Color.white)
                .frame(width: 12, height: 12)
                .offset(x: -5) // Ajuste la position horizontale
                .padding(.top, 50) // Ajuste la position verticale pour être centrée
            
            // Deuxième digit
            ZStack {
                Color.black
                    .frame(width: 60, height: 120)
                    .cornerRadius(10)
                
                let activeSegments = segments[digit2]
                
                ForEach(0..<7) { index in
                    SegmentView(isActive: activeSegments[index], index: index)
                }
            }
            
            // Troisième digit
            ZStack {
                Color.black
                    .frame(width: 60, height: 120)
                    .cornerRadius(10)
                
                let activeSegments = segments[digit3]
                
                ForEach(0..<7) { index in
                    SegmentView(isActive: activeSegments[index], index: index)
                }
            }
        }
        .padding(5) // Ajuste l'espacement global autour des digits
    }
}

struct SegmentView: View {
    let isActive: Bool
    let index: Int
    
    // Positions fixes pour chaque segment dans un écran 60x120
    private let positions: [CGRect] = [
        CGRect(x: 10, y: 0, width: 40, height: 10),  // Segment A (haut)
        CGRect(x: 50, y: 20, width: 10, height: 30), // Segment B (haut-droite)
        CGRect(x: 50, y: 60, width: 10, height: 30), // Segment C (bas-droite)
        CGRect(x: 10, y: 100, width: 40, height: 10), // Segment D (bas)
        CGRect(x: 0, y: 60, width: 10, height: 30),  // Segment E (bas-gauche)
        CGRect(x: 0, y: 20, width: 10, height: 30),  // Segment F (haut-gauche)
        CGRect(x: 10, y: 50, width: 40, height: 10)  // Segment G (milieu)
    ]
    
    var body: some View {
        let position = positions[index]
        
        return Rectangle()
            .fill(isActive ? Color.green : Color.gray.opacity(0.3))
            .frame(width: position.width, height: position.height)
            .position(x: position.midX+10 , y: position.midY ) // Centrer chaque segment
    }
}
