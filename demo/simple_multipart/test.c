#include <stdio.h>
#include <string.h>
#include "../../TinyFrame.h"
#include "../utils.h"

TinyFrame *demo_tf;

extern const char *romeo;

/**
 * This function should be defined in the application code.
 * It implements the lowest layer - sending bytes to UART (or other)
 */
void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len)
{
    printf("--------------------\n");
    printf("\033[32mTF_WriteImpl - sending frame:\033[0m\n");
    dumpFrame(buff, len);

    // Send it back as if we received it
    TF_Accept(tf, buff, len);
}

/** An example listener function */
TF_Result myListener(TinyFrame *tf, TF_Msg *msg)
{
    (void)tf;
    dumpFrameInfo(msg);
    if (strcmp((const char *) msg->data, romeo) == 0) {
        printf("FILE TRANSFERRED OK!\r\n");
    }
    else {
        printf("FAIL!!!!\r\n");
    }
    return TF_STAY;
}

void main(void)
{
    TF_Msg msg;

    // Set up the TinyFrame library
    demo_tf = TF_Init(TF_MASTER); // 1 = master, 0 = slave
    TF_AddGenericListener(demo_tf, myListener);

    printf("------ Simulate sending a LOOONG message --------\n");

    // We prepare a message without .data but with a set .len
    TF_ClearMsg(&msg);
    msg.type = 0x22;
    msg.len = (TF_LEN) strlen(romeo);
    
    // Start the multipart frame
    TF_Send_Multipart(demo_tf, &msg);
    
    // Now we send the payload in as many pieces as we like.
    // Careful - TF transmitter is locked until we close the multipart frame
    
    uint32_t remain = strlen(romeo);
    const uint8_t* toSend = (const uint8_t*)romeo;
    
    while (remain > 0) {
      uint32_t chunk = (remain>16) ? 16 : remain;
      
      // Send a piece
      TF_Multipart_Payload(demo_tf, toSend, chunk);
      
      remain -= chunk;
      toSend += chunk;
    }
    
    // Done, close
    TF_Multipart_Close(demo_tf);
}

const char *romeo = "THE TRAGEDY OF ROMEO AND JULIET\n"
    "\n"
    "by William Shakespeare\n"
    "\n"
    "\n"
    "\n"
    "Dramatis Personae\n"
    "\n"
    "  Chorus.\n"
    "\n"
    "\n"
    "  Escalus, Prince of Verona.\n"
    "\n"
    "  Paris, a young Count, kinsman to the Prince.\n"
    "\n"
    "  Montague, heads of two houses at variance with each other.\n"
    "\n"
    "  Capulet, heads of two houses at variance with each other.\n"
    "\n"
    "  An old Man, of the Capulet family.\n"
    "\n"
    "  Romeo, son to Montague.\n"
    "\n"
    "  Tybalt, nephew to Lady Capulet.\n"
    "\n"
    "  Mercutio, kinsman to the Prince and friend to Romeo.\n"
    "\n"
    "  Benvolio, nephew to Montague, and friend to Romeo\n"
    "\n"
    "  Tybalt, nephew to Lady Capulet.\n"
    "\n"
    "  Friar Laurence, Franciscan.\n"
    "\n"
    "  Friar John, Franciscan.\n"
    "\n"
    "  Balthasar, servant to Romeo.\n"
    "\n"
    "  Abram, servant to Montague.\n"
    "\n"
    "  Sampson, servant to Capulet.\n"
    "\n"
    "  Gregory, servant to Capulet.\n"
    "\n"
    "  Peter, servant to Juliet's nurse.\n"
    "\n"
    "  An Apothecary.\n"
    "\n"
    "  Three Musicians.\n"
    "\n"
    "  An Officer.\n"
    "\n"
    "\n"
    "  Lady Montague, wife to Montague.\n"
    "\n"
    "  Lady Capulet, wife to Capulet.\n"
    "\n"
    "  Juliet, daughter to Capulet.\n"
    "\n"
    "  Nurse to Juliet.\n"
    "\n"
    "\n"
    "  Citizens of Verona; Gentlemen and Gentlewomen of both houses;\n"
    "    Maskers, Torchbearers, Pages, Guards, Watchmen, Servants, and\n"
    "    Attendants.\n"
    "\n"
    "                            SCENE.--Verona; Mantua.\n"
    "\n"
    "\n"
    "\n"
    "                        THE PROLOGUE\n"
    "\n"
    "                        Enter Chorus.\n"
    "\n"
    "\n"
    "  Chor. Two households, both alike in dignity,\n"
    "    In fair Verona, where we lay our scene,\n"
    "    From ancient grudge break to new mutiny,\n"
    "    Where civil blood makes civil hands unclean.\n"
    "    From forth the fatal loins of these two foes\n"
    "    A pair of star-cross'd lovers take their life;\n"
    "    Whose misadventur'd piteous overthrows\n"
    "    Doth with their death bury their parents' strife.\n"
    "    The fearful passage of their death-mark'd love,\n"
    "    And the continuance of their parents' rage,\n"
    "    Which, but their children's end, naught could remove,\n"
    "    Is now the two hours' traffic of our stage;\n"
    "    The which if you with patient ears attend,\n"
    "    What here shall miss, our toil shall strive to mend.\n"
    "                                                         [Exit.]\n"
    "\n"
    "\n"
    "\n"
    "\n"
    "ACT I. Scene I.\n"
    "Verona. A public place.\n"
    "\n"
    "Enter Sampson and Gregory (with swords and bucklers) of the house\n"
    "of Capulet.\n"
    "\n"
    "\n"
    "  Samp. Gregory, on my word, we'll not carry coals.\n"
    "\n"
    "  Greg. No, for then we should be colliers.\n"
    "\n"
    "  Samp. I mean, an we be in choler, we'll draw.\n"
    "\n"
    "  Greg. Ay, while you live, draw your neck out of collar.\n"
    "\n"
    "  Samp. I strike quickly, being moved.\n"
    "\n"
    "  Greg. But thou art not quickly moved to strike.\n"
    "\n"
    "  Samp. A dog of the house of Montague moves me.\n"
    "\n"
    "  Greg. To move is to stir, and to be valiant is to stand.\n"
    "    Therefore, if thou art moved, thou runn'st away.\n"
    "\n"
    "  Samp. A dog of that house shall move me to stand. I will take\n"
    "    the wall of any man or maid of Montague's.\n"
    "\n"
    "  Greg. That shows thee a weak slave; for the weakest goes to the\n"
    "    wall.\n"
    "\n"
    "  Samp. 'Tis true; and therefore women, being the weaker vessels,\n"
    "    are ever thrust to the wall. Therefore I will push Montague's men\n"
    "    from the wall and thrust his maids to the wall.\n"
    "\n"
    "  Greg. The quarrel is between our masters and us their men.\n"
    "\n"
    "  Samp. 'Tis all one. I will show myself a tyrant. When I have\n"
    "    fought with the men, I will be cruel with the maids- I will cut off\n"
    "    their heads.\n"
    "\n"
    "  Greg. The heads of the maids?\n"
    "\n"
    "  Samp. Ay, the heads of the maids, or their maidenheads.\n"
    "    Take it in what sense thou wilt.\n"
    "\n"
    "  Greg. They must take it in sense that feel it.\n"
    "\n"
    "  Samp. Me they shall feel while I am able to stand; and 'tis known I\n"
    "    am a pretty piece of flesh.\n"
    "\n"
    "  Greg. 'Tis well thou art not fish; if thou hadst, thou hadst\n"
    "    been poor-John. Draw thy tool! Here comes two of the house of\n"
    "    Montagues.\n"
    "\n"
    "           Enter two other Servingmen [Abram and Balthasar].\n"
    "\n"
    "\n"
    "  Samp. My naked weapon is out. Quarrel! I will back thee.\n"
    "\n"
    "  Greg. How? turn thy back and run?\n"
    "\n"
    "  Samp. Fear me not.\n"
    "\n"
    "  Greg. No, marry. I fear thee!\n"
    "\n"
    "  Samp. Let us take the law of our sides; let them begin.\n"
    "\n"
    "  Greg. I will frown as I pass by, and let them take it as they list.\n"
    "\n"
    "  Samp. Nay, as they dare. I will bite my thumb at them; which is\n"
    "    disgrace to them, if they bear it.\n"
    "\n"
    "  Abr. Do you bite your thumb at us, sir?\n"
    "\n"
    "  Samp. I do bite my thumb, sir.\n"
    "\n"
    "  Abr. Do you bite your thumb at us, sir?\n"
    "\n"
    "  Samp. [aside to Gregory] Is the law of our side if I say ay?\n"
    "\n"
    "  Greg. [aside to Sampson] No.\n"
    "\n"
    "  Samp. No, sir, I do not bite my thumb at you, sir; but I bite my\n"
    "    thumb, sir.\n"
    "\n"
    "  Greg. Do you quarrel, sir?\n"
    "\n"
    "  Abr. Quarrel, sir? No, sir.\n"
    "\n"
    "  Samp. But if you do, sir, am for you. I serve as good a man as\n"
    "    you.\n"
    "\n"
    "  Abr. No better.\n"
    "\n"
    "  Samp. Well, sir.\n"
    "\n"
    "                        Enter Benvolio.\n"
    "\n"
    "\n"
    "  Greg. [aside to Sampson] Say 'better.' Here comes one of my\n"
    "    master's kinsmen.\n"
    "\n"
    "  Samp. Yes, better, sir.\n"
    "\n"
    "  Abr. You lie.\n"
    "\n"
    "  Samp. Draw, if you be men. Gregory, remember thy swashing blow.\n"
    "                                                     They fight.\n"
    "\n"
    "  Ben. Part, fools! [Beats down their swords.]\n"
    "    Put up your swords. You know not what you do.\n"
    "\n"
    "                          Enter Tybalt.\n"
    "\n"
    "\n"
    "  Tyb. What, art thou drawn among these heartless hinds?\n"
    "    Turn thee Benvolio! look upon thy death.\n"
    "\n"
    "  Ben. I do but keep the peace. Put up thy sword,\n"
    "    Or manage it to part these men with me.\n"
    "\n"
    "  Tyb. What, drawn, and talk of peace? I hate the word\n"
    "    As I hate hell, all Montagues, and thee.\n"
    "    Have at thee, coward!                            They fight.\n"
    "\n"
    "     Enter an officer, and three or four Citizens with clubs or\n"
    "                          partisans.\n"
    "\n"
    "\n"
    "  Officer. Clubs, bills, and partisans! Strike! beat them down!\n"
    "\n"
    "  Citizens. Down with the Capulets! Down with the Montagues!\n"
    "\n"
    "           Enter Old Capulet in his gown, and his Wife.\n"
    "\n"
    "\n"
    "  Cap. What noise is this? Give me my long sword, ho!\n"
    "\n"
    "  Wife. A crutch, a crutch! Why call you for a sword?\n"
    "\n"
    "  Cap. My sword, I say! Old Montague is come\n"
    "    And flourishes his blade in spite of me.\n"
    "\n"
    "                 Enter Old Montague and his Wife.\n"
    "\n"
    "\n"
    "  Mon. Thou villain Capulet!- Hold me not, let me go.\n"
    "\n"
    "  M. Wife. Thou shalt not stir one foot to seek a foe.\n"
    "\n"
    "                Enter Prince Escalus, with his Train.\n"
    "\n"
    "\n"
    "END OF FILE\n";
