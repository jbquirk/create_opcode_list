/****************************************************************************
 *                                                                          *
 * File    : main.c                                                         *
 *                                                                          *
 * Purpose : Console mode (command line) program.                           *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
/****************************************************************************
 *                                                                          *
 * Function: main                                                           *
 *                                                                          *
 * Purpose : Main entry point.                                              *
 *                                                                          *
 * History : Date      Reason                                               *
 *           00/00/00  Created                                              *
 *                                                                          *
 ****************************************************************************/
// Note once a terminal is reached it will generate a code when combined with dest will
// point to hex op code and info to generate instruction.
#define NUM_OP 256
#define OP_SPACE NUM_OP/5 //This is give space to allow the opcode tree to be bulit
#define OPCODE_AR 2048    //A space 4 times the size to store opcodes.
#define SYMBOL_SZ 512      //I don't think there will be more than 40 instructions
// Global Varaiables

struct inst_node {
				char *inst;   // root instruction
				struct reg_node *src;  // src combinations
				struct reg_node *dest; // dest
 				int index; //these values point to the link table 
				} ;

struct reg_node {
				struct reg_node *r; // pointer to next chain NULL if end
				char *reg; // VAL means a val this should have a size
				int type;  // various types constant, point, inc or dec pointer 
				int val;
				int size; //Size can 0,8,16
				int index; //these values point to the link table
				} ;

struct o_node {
				char *inst; 
				char *src;
				int src_t;
				char *dest;
				int dest_t;
				int op_code; //Actual opcode for this instruction 
				int size;    //Size of Value, 0,8,16
				} ;

struct reg {
			char *name;
			int size; // code will try and guess size -1 unkown
			int type; // again will max type reg,preg incp
		 } regs[20]; 
			  	
int op_codes[OPCODE_AR]; //space for opcode storage
int OpCodeIndex = 0;      // This hold the value of the opcode index 
struct o_node *symbol_t[SYMBOL_SZ] ; //Create instruction table 

// Declarations
void buildTree(char *op,char *cnt,char *hex);
struct o_node processOp(char *op); 
void printTree(void);
enum type { reg,reg16,number,pointer,preg,incp,decp,none,bad };
char *atype[] = { "reg","reg16","number","pointer","preg","incp","decp","none","bad" }; 
int getType(char *);
char *strip_mod(char *s );
void displayOp(struct o_node op_node);
int convert_num(char *buf);
int get_size(char *s);
void insert_node(struct o_node op_node); 
struct reg_node *scan_node(struct reg_node *n);
void ins_reg_node(struct reg_node *n, char *c, int type, int size);
void updateRegInfo(char *r,int type);
void dump_reg(void);
void copy_op_node(struct o_node *d, struct o_node s);
void dump_tree(void);
bool checkIns(char *s[], char *d);
void sort_regs(void);
int count_reg(void);

FILE *fp2;

int main(int argc, char *argv[])
{
	char buf[128];
    size_t bufsize = 128;
	char hex[6],*op,*cnt;
	bool csv = false,header = true;
	int index=0,i2,opt;
 	FILE *fp1;
	fp2=stdout;
	while ((opt = _getopt(argc, argv, "-:o:hx")) != -1) 
  {
     switch (opt) 
     {
      case 'o':
        fprintf(stderr,"Option o was provided with arg %s\n",optarg);
   		fp2 = fopen(optarg , "w"); //the output file
   		if(fp2 == NULL) {
      		perror("Error opening file");
      	return(-1);
	  }
        break;
      case 'h':
        fprintf(stderr,"Option h was provided\n");
		header =false; // supress header output
        break;
      case 'x':
        fprintf(stderr,"Option X was provided\n");
		csv = true;
        break;
     case '?':
        fprintf(stderr,"Unknown option: %c\n", optopt);
        break;
      case ':':
        fprintf(stderr,"Missing arg for %c\n", optopt);
        break;
      case 1:
        fprintf(stderr,"Non-option arg: %s\n", optarg);
 		fp1 = fopen(optarg , "r");
   		if(fp1 == NULL) {
     		 perror("Error opening file");
      		return(-1);
			}
        break;
     }
  }
   
   /* opening file for reading */

 	while(fgets(buf,bufsize,fp1)!=NULL) // this program assumes the micro code has complied correctly
	{
		if(strncmp(buf,"op:",3)== 0) // found def
		{
			i2 = 0;
			index = index +3;
			while(isspace(buf[index]))
					index++;
			while(buf[index] !=';')
				hex[i2++] = buf[index++];
			while(buf[index]!='/')
				index++;
			index = index + 2;
			while(isspace(buf[index]))
					index++;
			op = buf+index;
			while(!isspace(buf[index]))
					index++;
			buf[index++] = '\0';
			while(!isdigit(buf[index]))
					index++; // look for digit
			cnt = &buf[index++];
			buf[index] ='\0';
			if(csv) // output if -x
				fprintf(fp2,"\t\"%s\",%s,\t%s,\t\t//%s",op,cnt,hex,&buf[index+1]);
			buildTree(op,cnt,hex);
			index=0;
			i2=0;
		}
	}
	if (header){
		dump_reg();
		dump_tree();
	}

    return 0;
}
void buildTree(char *op,char *cnt,char *hex)
{
	struct o_node op_node;
	op_node = processOp(op); // Analyse op string and break into instruction
	op_node.op_code = convert_num(hex);
	op_node.size = get_size(cnt);

// Ok we have extracted all the info in the primitive mnemonics now lets bulid the proto parse 
// Tree.
  	insert_node(op_node); 

//	displayOp(op_node); // this is just a test routine
// Next we take the processed instruction and add in size and actual opcode

}
// This routine will break the insturction into field for the other steps to process
// op is in the form of INST_SRC_DEST 
// It also works out if SRC or DEST are values or pointers 
struct o_node processOp(char *op)
{
		struct o_node op_node;
		op_node.inst = op;  // this will always point instruction
//		printf("Here at Process op\n");
		while(*op != '_' && *op !='\0')
			op++;
		if( *op == '\0') 
		{ // single instruction no src or dest
			op_node.src = NULL;
			op_node.src_t = none;
			op_node.dest = NULL;
			op_node.dest_t = none;
			return(op_node); 
		} else
			*op = '\0';
		op++; //Should now point to source
		op_node.src = op;
		while(*op != '_' && *op !='\0')
			op++;
		if( *op == '\0') 
		{ // src only no dest
			op_node.src_t = getType(op_node.src); // further processing required
			op_node.src = strip_mod(op_node.src); //remove any modifier if present
			updateRegInfo(op_node.src,op_node.src_t);  
			op_node.dest = NULL;
			op_node.dest_t = none;
			return(op_node); 
		}
// if here full instruct with 
		*op = '\0';
		op_node.src_t = getType(op_node.src); // further processing required 
		op_node.src = strip_mod(op_node.src); //remove any modifier if present
		updateRegInfo(op_node.src,op_node.src_t);  
		op++; //Should now point to dest
		op_node.dest = op;
		op_node.dest_t = getType(op_node.dest); // further processing required 
		op_node.dest = strip_mod(op_node.dest); //remove any modifier if present
		updateRegInfo(op_node.dest,op_node.dest_t);

	return(op_node);
}
//This works out type from string passed and returns enum of type 
int getType(char *s)
{
	char *p;
	p = s; // save position of s
	// Check for (
//	printf("Here at get type\n");
	if ( *s == '(' )
		{ // we have a pointer type
		while(++s !='\0'){
			if(*s == '+') // have a inc pointer
				{
				++s;
				if(*s == ')') // we are valid
					return(incp); //These are required to be REG
				else
					return(bad);
				} 
			if(*s == '-') // have a dec pointer currently not used
				{
				++s;
				if(*s == ')') // we are valid
					return(decp); //These are required to be REG
				else
					return(bad);
				} 
			if(*s == ')') // have a normal pointer
				{
				++p;
				if(*p == 'V') 
					return(pointer);
				else
					return(preg); // A register is is used as a pointer
				}
			}
		return(bad); // if we get here something wrong
		}
// if here must be just a plain value or reg
// Next we do a basic check if the first letter V its a number else its a reg
	if(*p == 'V' ) // its a Value
		return(number);
	else
		return(reg);
}
// Returns *s as only text 
char *strip_mod(char  *s)
{
	char *p;
//	printf("Strip mod s = %s \n",s); 
	// first check if has mod
	if(*s != '(' ) // has mod return
		return(s);
	s++;
	p=s; 
//	printf("Strip mod p = %s \n",p); 
	while( *s != '\0')
	{
		if(*s == '+'){
			*s = '\0';
			return(p);
		}
		if(*s == '-'){
			*s = '\0';
			return(p);
		}
		if(*s == ')'){
			*s = '\0';
			return(p);
		}
		s++;
	}
	return(NULL);
}
// decodes the op_node past
void displayOp(struct o_node op_node)
{
			fprintf(fp2,"Inst: %s: OPCODE %x: size %d \n", op_node.inst,op_node.op_code,op_node.size);
			if(op_node.src != NULL)
				fprintf(fp2,"Source: %s type: %s\n", op_node.src, atype[op_node.src_t]);
			if(op_node.dest != NULL)
				fprintf(fp2,"Dest: %s type: %s\n", op_node.dest, atype[op_node.dest_t]);

}
int convert_num(char *buf)
{
		long number;
		number = strtol(buf,NULL,0);
//		printf("found number %ld\n",num);
//		number;
		return((int)number); 
}
int get_size(char * s)
{
		int c;
		c = *s - '0'; 
		c--; //adjust size;
	return(c*8);
}
void insert_node(struct o_node op_node)
{
	int i;
	static int i_index = 0;
	struct reg_node *n; 

	for ( i=0; i < SYMBOL_SZ; i++)
	{
		if( symbol_t[i] == NULL ) // we have found end of symbol table
		{	//create empty node
			symbol_t[i] = (struct o_node *)malloc(sizeof(struct o_node));
			copy_op_node(symbol_t[i],op_node);

			return; // All done for new
		}
		// All singles should have been caught above and unseen so we should alway expect to find 
		// Instruction here

	}
}

// Scan node looking for last node.

struct reg_node *scan_node(struct reg_node *n )
{
		struct reg_node *s;
		s = n;
		if (n->r == NULL)
			return(n);	
		while(s->r != NULL)
			s = n->r;
		return(s);
}
// insert reg node 
// This will create node based on values sent
// It assumes existance checks have been done.
void ins_reg_node(struct reg_node *n, char *c, int type, int size )
{
	struct reg_node *n1;

			n1 = scan_node(n->r);
			n1->r = (struct reg_node *)malloc(sizeof(struct reg_node));
			n1 = n1->r; //Get new node 
			n->reg = (char *)malloc(sizeof(c));
			strcpy(n->reg,c);
			n->type = type;
			if(type == number )
				n->size = size; 
			else 
				n->size = 0;
}
// This builds a table of all found registers 
// Also takes a guess at there size
void updateRegInfo(char *r,int type)
{
		int i;
	if(type == number)
		return; //No reg value
	for (i =0; i < 20; i++)
	{
			if( regs[i].name == NULL ) // Reg not found
				{
				regs[i].name = (char *)malloc(sizeof(char)*strlen(r));
				strcpy(regs[i].name,r);
				regs[i].type = type;
				if(type > number)
					regs[i].size =16;
				else
					regs[i].size = 8;
				return;
				}
			if(strcmp(regs[i].name,r) == 0)
				{
				if(type > regs[i].type)
						regs[i].type = type; 
				if((type == reg16) && (regs[i].size ==8))
						regs[i].size =16;
				return ; //reg defined
				}
	}
 	
}
void dump_reg(void)
{
	int i,j;
	i =j=0;
	sort_regs();
	fprintf(fp2,"enum type { reg,reg16,number,pointer,preg,incp,decp,none,bad,instruction };\n");
	fprintf(fp2,"enum regs {");
	while(regs[i].name != NULL)
	{
		fprintf(fp2,"reg%s,",regs[i].name);
		i++;
		if(regs[i].size == 16)
			j++; //count 16 bit registers
	}
	fprintf(fp2,"regVAL };\n");
	fprintf(fp2,"#define NUM_REG %d\n", i); 
	fprintf(fp2,"#define NUM_16_REG %d\n", j); 
	i=0;
	fprintf(fp2,"struct registers {\n \tchar *name; \n \tint size; \n \tint code; \n \tint type;\n } ;\n ");
	fprintf(fp2,"struct registers r[NUM_REG+1] = { \n");
		 
	while(regs[i].name != NULL)
	{
		fprintf(fp2,"\t\"%s\",%d,reg%s,%s,\n",regs[i].name,regs[i].size,regs[i].name,atype[regs[i].type]);
		i++;
	} 
	fprintf(fp2,"\t \"VAL\",-1,regVAL,number };\n");
	

}
void copy_op_node(struct o_node *d, struct o_node s)
{
			d->inst = (char*)malloc(sizeof(char*)*strlen(s.inst));
			strcpy(d->inst,s.inst); // insert instruction
			d->op_code = s.op_code;
			d->size = s.size;
			if(s.src == NULL && s.dest == NULL ) // this is singlet so finish up
			{
				 // This is the terminal point so set nodes to null
				d->src = NULL;
				d->dest = NULL;
				return;
			}
		// If here must have least src
			d->src = (char *)malloc(sizeof(char *)* strlen(s.src));
			strcpy(d->src,s.src);
			d->src_t = s.src_t;
//			printf("node: %s reg: %s\n",d->inst,d->src);
 			if(s.dest != NULL ) //we have a triple
			{
				d->dest = (char *)malloc(sizeof(char*) * strlen(s.dest));
				strcpy(d->dest,s.dest);
				d->dest_t = s.dest_t;
			}
			else
				d->dest = NULL;
			if(d->size == 16 && d->dest_t == number ) // we have op with 16 bit reg
			  updateRegInfo(d->src,reg16); // re-call with src and force 16 bit  
			return; // All done for new
}
//This code does two passes though the gathered opcodes.
void dump_tree(void)
{
	static char *ops[80]; // holds the op codes as found.
	int i=0,j=0, num_ops=0; //temp loop var and counter for total number of operators.	
	// these variable will hold info about currently processing instruction they are used 
	// in pass 2
	int operands; //how many oprands for this instruction
	bool has_imed; // Can accept imediate vales 
	bool has_address; // has address modifiers
	int count_v;

	// pass 1;
	while(symbol_t[i]!= NULL)
		{ 
		ops[num_ops] = NULL; //ensure a NULL always marke the end;
		if(num_ops == 0) // this is first inst encountered.
			ops[num_ops++] = symbol_t[i]->inst;
		else
			if( !checkIns(ops,symbol_t[i]->inst) )
				{ // returns true if opcode found 
				ops[num_ops++] = symbol_t[i]->inst;
//				printf("Adding instruction %s\n",symbol_t[i]->inst);
				}
		i++;		
				
		}
// Pass one complete we now have all instructions now to process
	for(i=0; i< num_ops; i++){ // process each instruction
//		printf("%s :",ops[i]);
		j=0;
		operands = count_v =0;
		has_imed =has_address = false;

		while(symbol_t[j] != NULL){
			if(strcmp(ops[i],symbol_t[j]->inst) == 0)
				{
//				printf("%0X %d",symbol_t[j]->op_code,symbol_t[j]->size);
				count_v++;
				if(symbol_t[j]->src == NULL && symbol_t[j]->dest == NULL)
							operands =0; // single byte instruction
				if(symbol_t[j]->src != NULL)
					{
//					printf("Source: %s type: %s ", symbol_t[j]->src, atype[symbol_t[j]->src_t]);
					operands = 1;
					if(symbol_t[j]->src_t == number)
						has_imed = true;
					if(symbol_t[j]->src_t > number) 
							has_address = true; 
					}
				if(symbol_t[j]->dest != NULL)
					{
//					printf("Dest: %s type: %s ", symbol_t[j]->dest, atype[symbol_t[j]->dest_t]);
					operands = 2;
					if(symbol_t[j]->dest_t == number)
						has_imed = true;
					if(symbol_t[j]->dest_t > number)
							has_address = true;
					}
//				printf("\n");
				}
			j++;
			}
//			printf("intruction has %d operand%s %s and variable length\n",operands,operands > 1? "s":"", has_imed==true? "are":"are not");
//			if(has_address)
//				printf("Allows address modifiers\n");
//			printf("and has %d combinations \n",count_v);

		}
	fprintf(fp2,"#define NUMBEROPS  %d \n",num_ops);
	fprintf(fp2,"#define NUMBERINST  %d \n",j);
	fprintf(fp2,"enum instr {"); 
	for(i=0; i< num_ops; i++) // process each instruction
		fprintf(fp2,"%s,",ops[i]);
	fprintf(fp2,"inst_end };\n");
	fprintf(fp2,"char *a_inst[NUMBEROPS+1] = {");
	for(i=0; i< num_ops; i++) // process each instruction
		fprintf(fp2,"\"%s\",",ops[i]);
	fprintf(fp2,"\"inst_end\" };\n");
	fprintf(fp2,"struct op_code {\n\t int inst; \n\t int size;\n\t int opcode;\n\tint src;\n\t int src_t;\n");
	fprintf(fp2,"\t int dest;\n\t int dest_t;\n\t int operands; \n\t\t };\n");
	fprintf(fp2,"struct op_code op_codes[NUMBERINST+1] = {\n");
	for(i=0; i< num_ops; i++){ // dump each instruction in format ready for parser
		j=0;
		operands = count_v =0;
		has_imed =has_address = false;
		while(symbol_t[j] != NULL){
			if(strcmp(ops[i],symbol_t[j]->inst) == 0)
				{
				fprintf(fp2,"\t%s,",ops[i]);
				fprintf(fp2,"%d,%d,",symbol_t[j]->size,symbol_t[j]->op_code);
				count_v++;
				if(symbol_t[j]->src == NULL && symbol_t[j]->dest == NULL)
							operands =0; // single byte instruction
				if(symbol_t[j]->src != NULL)
					{
					fprintf(fp2,"reg%s,%s,", symbol_t[j]->src, atype[symbol_t[j]->src_t]);
					operands = 1;
					}
				else
					fprintf(fp2,"none,none,");
				if(symbol_t[j]->dest != NULL)
					{
					fprintf(fp2,"reg%s,%s,", symbol_t[j]->dest, atype[symbol_t[j]->dest_t]);
					operands = 2;
					}
				else
					fprintf(fp2,"none,none,");
			fprintf(fp2,"%d,\n",operands);
				}
			j++;
			}

		}
	fprintf(fp2,"\tinst_end,0,0,0,0,0,0,0 };\n");
}
	//
	//
bool checkIns(char *s[], char *d)
{
	int i;
	for ( i=0; i < 80; i++){
		if(s[i] == NULL)
			return false;
		if(strcmp(s[i],d)==0)
			return true;
		}
}
void sort_regs(void)
{
	int i,j;
	struct reg rtemp;
	bool swap = true;
		j = count_reg();

		while(swap != false)
			{
			swap = false;
			for (i = 0; i < j-1; i++) //sort alpha first
				if (strcmp(regs[i].name,regs[i+1].name) > 0 )
					{
					rtemp= regs[i];
					regs[i] =regs[i+1];
					regs[i+1] = rtemp;
					swap = true;
					}
			}
		swap = true;
		while(swap != false) //Now sort size
			{
			swap = false;
			for (i = 0; i < j-1; i++)
				if(regs[i].size > regs[i+1].size) // swap
					{
					rtemp= regs[i];
					regs[i] =regs[i+1];
					regs[i+1] = rtemp;
					swap = true;
					}
						
			}
}
int count_reg(void)
{
	int count = 0;
		while(regs[count].name != NULL)
			count++;
	return(count);	
}
