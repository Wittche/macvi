import sys

def main():
    if len(sys.argv) != 4:
        print("Usage: modify_asm.py <header> <infile> <outfile>")
        sys.exit(1)
    
    header = sys.argv[1].replace('\\n', '\n')
    infile = sys.argv[2]
    outfile = sys.argv[3]
    
    with open(infile, 'r', encoding='utf-8') as f:
        content = f.read()
        
    with open(outfile, 'w', encoding='utf-8') as f:
        f.write(header + "\n" + content + "\nret\n")

if __name__ == "__main__":
    main()
