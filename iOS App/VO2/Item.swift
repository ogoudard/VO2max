//
//  Item.swift
//  VO2
//
//  Created by Goudard Olivier on 31/10/2024.
//

import Foundation
import SwiftData

@Model
final class Item {
    var timestamp: Date
    
    init(timestamp: Date) {
        self.timestamp = timestamp
    }
}
