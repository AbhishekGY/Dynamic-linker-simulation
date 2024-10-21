#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <array>
#include <iomanip>

// Virtual memory constants
constexpr size_t PAGE_SIZE = 256;  // 256 bytes per page
constexpr size_t NUM_PAGES = 256;  // 256 pages, giving us a 64KB address space
constexpr size_t MEMORY_SIZE = PAGE_SIZE * NUM_PAGES;

// Virtual Memory System
class VirtualMemory {
private:
    std::array<uint8_t, MEMORY_SIZE> memory;
    std::array<bool, NUM_PAGES> page_table;
    size_t next_free_page = 0;

public:
    VirtualMemory() : memory{}, page_table{} {}

    uint64_t allocate(size_t size) {
        size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        size_t start_page = next_free_page;

        for (size_t i = 0; i < pages_needed; ++i) {
            if (next_free_page >= NUM_PAGES) {
                throw std::runtime_error("Out of memory");
            }
            page_table[next_free_page++] = true;
        }

        return start_page * PAGE_SIZE;
    }

    void write(uint64_t address, const void* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            size_t page = (address + i) / PAGE_SIZE;
            size_t offset = (address + i) % PAGE_SIZE;

            if (!page_table[page]) {
                throw std::runtime_error("Segmentation fault: writing to unallocated memory");
            }

            memory[address + i] = static_cast<const uint8_t*>(data)[i];
        }
    }

    void read(uint64_t address, void* data, size_t size) const {
        for (size_t i = 0; i < size; ++i) {
            size_t page = (address + i) / PAGE_SIZE;
            size_t offset = (address + i) % PAGE_SIZE;

            if (!page_table[page]) {
                throw std::runtime_error("Segmentation fault: reading from unallocated memory");
            }

            static_cast<uint8_t*>(data)[i] = memory[address + i];
        }
    }

    void print_memory(uint64_t start, size_t size) const {
        std::cout << "Memory dump from 0x" << std::hex << start << " to 0x" << (start + size) << std::dec << ":\n";
        for (size_t i = 0; i < size; ++i) {
            if (i % 16 == 0) {
                std::cout << std::hex << std::setw(8) << std::setfill('0') << (start + i) << ": ";
            }
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(memory[start + i]) << " ";
            if ((i + 1) % 16 == 0) {
                std::cout << "\n";
            }
        }
        std::cout << std::dec << std::endl;
    }
};

// Global virtual memory instance
VirtualMemory vm;

// Represents a symbol (function or variable)
class Symbol {
public:
    Symbol(const std::string& name, uint64_t address) : name(name), address(address) {}
    std::string name;
    uint64_t address;
};

// Represents a relocation entry
class Relocation {
public:
    Relocation(const std::string& symbolName, uint64_t offset) 
        : symbolName(symbolName), offset(offset) {}
    std::string symbolName;
    uint64_t offset;
};

// Represents a shared library
class Library {
public:
    Library(const std::string& name) : name(name), base_address(vm.allocate(PAGE_SIZE)) {}
    void addSymbol(const std::string& name, uint64_t offset) {
        uint64_t address = base_address + offset;
        symbols[name] = std::make_shared<Symbol>(name, address);
        // Write some dummy data to represent the function
        uint64_t dummy_func = 0xDEADBEEF;
        vm.write(address, &dummy_func, sizeof(dummy_func));
    }
    std::shared_ptr<Symbol> findSymbol(const std::string& name) {
        auto it = symbols.find(name);
        return (it != symbols.end()) ? it->second : nullptr;
    }
    std::string name;
    uint64_t base_address;
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;
};

// Represents an executable
class Executable {
public:
    Executable(const std::string& name) : name(name), base_address(vm.allocate(PAGE_SIZE)) {}
    void addDependency(std::shared_ptr<Library> lib) {
        dependencies.push_back(lib);
    }
    void addRelocation(const std::string& symbolName, uint64_t offset) {
        relocations.emplace_back(symbolName, offset);
    }
    std::string name;
    uint64_t base_address;
    std::vector<std::shared_ptr<Library>> dependencies;
    std::vector<Relocation> relocations;
    std::unordered_map<std::string, uint64_t> symbolAddresses;
};

// Represents the dynamic linker
class DynamicLinker {
public:
    void loadLibrary(std::shared_ptr<Library> lib) {
        loadedLibraries[lib->name] = lib;
    }
    
    void linkExecutable(std::shared_ptr<Executable> exe) {
        std::cout << "Linking " << exe->name << "...\n";
        
        // Load all dependencies
        for (const auto& dep : exe->dependencies) {
            if (loadedLibraries.find(dep->name) == loadedLibraries.end()) {
                loadLibrary(dep);
                std::cout << "Loaded library: " << dep->name << " at address 0x" << std::hex << dep->base_address << std::dec << "\n";
            }
        }
        
        // Resolve symbols and perform relocations
        for (const auto& relocation : exe->relocations) {
            std::shared_ptr<Symbol> resolvedSymbol = nullptr;
            
            // Try to find the symbol in loaded libraries
            for (const auto& lib : loadedLibraries) {
                resolvedSymbol = lib.second->findSymbol(relocation.symbolName);
                if (resolvedSymbol) {
                    std::cout << "Resolved symbol: " << relocation.symbolName 
                              << " from " << lib.first << "\n";
                    break;
                }
            }
            
            if (!resolvedSymbol) {
                throw std::runtime_error("Unresolved symbol: " + relocation.symbolName);
            }
            
            // Perform relocation
            uint64_t relocatedAddress = resolvedSymbol->address;
            exe->symbolAddresses[relocation.symbolName] = relocatedAddress;
            
            // Write the resolved address to the executable's memory
            uint64_t relocationAddress = exe->base_address + relocation.offset;
            vm.write(relocationAddress, &relocatedAddress, sizeof(relocatedAddress));
            
            std::cout << "Relocated symbol: " << relocation.symbolName 
                      << " at address 0x" << std::hex << relocationAddress 
                      << " to point to 0x" << relocatedAddress << std::dec << "\n";
        }
        
        std::cout << "Linking completed for " << exe->name << "\n";
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Library>> loadedLibraries;
};

int main() {
    // Create some libraries
    auto libMath = std::make_shared<Library>("libmath.so");
    libMath->addSymbol("sqrt", 0x100);
    libMath->addSymbol("pow", 0x200);

    auto libGraphics = std::make_shared<Library>("libgraphics.so");
    libGraphics->addSymbol("draw_line", 0x100);
    libGraphics->addSymbol("draw_circle", 0x200);

    // Create an executable with unresolved symbols
    auto myApp = std::make_shared<Executable>("myapp");
    myApp->addDependency(libMath);
    myApp->addDependency(libGraphics);
    
    // Add relocations for unresolved symbols
    myApp->addRelocation("sqrt", 0x100);
    myApp->addRelocation("draw_line", 0x200);

    // Create a dynamic linker and link the executable
    DynamicLinker linker;
    try {
        linker.linkExecutable(myApp);
        
        // Print memory contents after linking
        vm.print_memory(myApp->base_address, 512);  // Print first two pages of the executable
    } catch (const std::exception& e) {
        std::cerr << "Linking failed: " << e.what() << "\n";
    }

    return 0;
}
