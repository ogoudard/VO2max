import SwiftUI

struct LungTestView: View {
    @State private var testStarted = false
    @State private var lungCapacity: Double = 0.0 // En litres
    
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
            
            Button(action: {
                testStarted.toggle()
                if testStarted {
                    startSimulation()
                } else {
                    lungCapacity = 0.0
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
        }
        .padding()
    }
    
    private func startSimulation() {
        lungCapacity = 0.0 // Reset à 0
        if testStarted {
            let interval = 15.0 / 50.0 // 50 étapes pour une montée douce sur 15 secondes
            var step = 0
            
            Timer.scheduledTimer(withTimeInterval: interval, repeats: true) { timer in
                if !testStarted { // Si le test est arrêté
                    timer.invalidate()
                    return
                }
                
                if step > 50 {
                    step = 0 // Repartir de 0
                }
                
                lungCapacity = Double(step) * 5.0 / 50.0 // Augmente doucement jusqu'à 5.0
                step += 1
            }
        }
    }
}

struct SevenSegmentDisplay: View {
    let value: Double
    
    var body: some View {
        VStack(spacing: 10) {
            HStack(spacing: 5) {
                let digits = digits(for: value) // Fonction pour extraire les digits
                if digits.count == 3 {
                    DigitView(digit1: digits[0], digit2: digits[1], digit3: digits[2]) // Passer les 3 digits
                }
            }
            .padding()
            Text("Liters")
                .font(.title)
                .foregroundColor(.green)
        }
    }
    
    private
    func digits(for value: Double) -> [Int] {
        // Assurez-vous d'extraire 3 chiffres. Exemple simple avec 3 digits après la virgule
        let roundedValue = Int(value * 100) // Multiplie la valeur par 100 pour garder 3 décimales
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
        HStack(spacing: 5) {
            // Premier digit
            ZStack {
                Color.black
                    .frame(width: 60, height: 120)
                    .cornerRadius(10)
                
                let activeSegments = segments[digit1]
                
                // Dessin des segments
                ForEach(0..<7) { index in
                    SegmentView(isActive: activeSegments[index], index: index)
                }
            }
            
            // Point entre les deux premiers digits
            ZStack {
                Circle()
                    .fill(Color.white)
                    .frame(width: 10, height: 10)
                    .position(x: 30, y: 60)
            }
            
            // Deuxième digit
            ZStack {
                Color.black
                    .frame(width: 60, height: 120)
                    .cornerRadius(10)
                
                let activeSegments = segments[digit2]
                
                // Dessin des segments
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
                
                // Dessin des segments
                ForEach(0..<7) { index in
                    SegmentView(isActive: activeSegments[index], index: index)
                }
            }
        }
    }
}

struct SegmentView: View {
    let isActive: Bool
    let index: Int
    
    // Positions des segments par index
    private let positions = [
        CGRect(x: 15, y: 0, width: 30, height: 10),  // Segment A (haut)
        CGRect(x: 45, y: 20, width: 10, height: 30), // Segment B (haut-droite)
        CGRect(x: 45, y: 60, width: 10, height: 30), // Segment C (bas-droite)
        CGRect(x: 15, y: 100, width: 30, height: 10), // Segment D (bas)
        CGRect(x: 0, y: 60, width: 10, height: 30),  // Segment E (bas-gauche)
        CGRect(x: 0, y: 20, width: 10, height: 30),  // Segment F (haut-gauche)
        CGRect(x: 15, y: 50, width: 30, height: 10)  // Segment G (milieu)
    ]
    
    var body: some View {
        let position = positions[index]
        
        return Rectangle()
            .fill(isActive ? Color.green.opacity(0.97) : Color.gray.opacity(0.25))  // Vert clair et gris foncé
            .frame(width: position.width, height: position.height)
            .position(x: position.midX, y: position.midY)
    }
}
