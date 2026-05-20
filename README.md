# Eliotopy V2

Tactical visual assistant for Eliotrope players in Dofus, designed for portal network management, distance calculation, and redirection planning.  
This is a complete rewrite of the original project [Eliotopy](https://github.com/Romain-P/Eliotopy).

## Transition from OCR to Memory Reading

The previous version relied on screen capture and OCR.  
Due to technical limitations, latency, and reliability issues, the project has been rewritten to access game data directly via memory reading.  
This provides real-time access to accurate data, such as full map layouts, Line of Sight (LOS), and walkable cells, without visual analysis errors.

## Technical Choices

### Memory Reading vs. MITM
While MITM is a standard approach, Dofus protocol obfuscation changes frequently with updates. Conversely, IL2CPP offsets remain stable across updates, providing a more reliable long-term solution.

### Security
To ensure account safety and avoid detection, memory reading is performed strictly externally. No code is injected into the game process.

## Status

- [x] Memory wrappers
- [x] Generic memory access logging
- [x] GameObject manager retrieval
- [x] Map data retrieval (out-of-combat):
    - [x] LOS information
    - [x] Walkable cells

## License

This project is intended for educational and personal use. Use at your own risk.