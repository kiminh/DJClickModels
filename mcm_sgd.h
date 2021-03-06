#ifndef MCM_SGD_H
#define MCM_SGD_H
//#define double long double
class mcm_sgd:public model
{
    public:
        double forward[DOCPERPAGE+2][2],backward[DOCPERPAGE+2][2];
        double gamma[DOCPERPAGE+2][DOCPERPAGE+2],cgamma[DOCPERPAGE+2][DOCPERPAGE+2];
        //alpha_cnt=train_cnt
        vector<double> alpha,beta,s_c,s_e,calpha,cbeta,cs_c,cs_e;
        double dlt=0.5,ddlt=0.7,eps=1e-9;
        double get_rel(int id)
        {
            return alpha[id];
        }
        void train_init()
        {
            name="Mcm-sgd";
            doc_rel=vector<double>(docs.size()+1);
            alpha=vector<double>(docs.size()+1);
            beta=vector<double>(MAXVERTICLE+1);
            s_e=vector<double>(docs.size()+1);
            s_c=vector<double>(docs.size()+1);
            calpha=vector<double>(docs.size()+1);
            cbeta=vector<double>(MAXVERTICLE+1);
            cs_e=vector<double>(docs.size()+1);
            cs_c=vector<double>(docs.size()+1);
            for(int i=0;i<=DOCPERPAGE;++i)
                for(int j=0;j<=DOCPERPAGE;++j)
                    gamma[i][j]=0.5;
            for(int i=0;i<=docs.size();++i)
            {
                alpha[i]=s_c[i]=s_e[i]=0.5;
            }
            for(int i=0;i<=MAXVERTICLE;++i)
                beta[i]=0.5;
            //train_clear();
        }
        void train_clear()
        {
            double pr=1;
            for(int i=0;i<=docs.size();++i)
            {
                calpha[i]=pr*(1./alpha[i]-1./(1.-alpha[i]));
                cs_c[i]=pr*(1./s_c[i]-1./(1.-s_c[i]));
                cs_e[i]=pr*(1./s_e[i]-1./(1.-s_e[i]));
            }
            for(int i=0;i<=MAXVERTICLE;++i)
                cbeta[i]=pr*(1./beta[i]-1./(1.-beta[i]));
            for(int i=0;i<=DOCPERPAGE;++i)
                for(int j=0;j<=DOCPERPAGE;++j)
                    cgamma[i][j]=pr*(1./gamma[i][j]-1./(1.-gamma[i][j]));
        }
        void train_update()
        {

            for(int i=0;i<docs.size();++i)
            {
                if(calpha[i]>eps)
                    alpha[i]=min(1.0-eps,alpha[i]+dlt);
                if(calpha[i]<-eps)
                    alpha[i]=max(eps,alpha[i]-dlt);
                if(cs_c[i]>eps)
                    s_c[i]=min(1.0-eps,s_c[i]+dlt);
                if(cs_c[i]<-eps)
                    s_c[i]=max(eps,s_c[i]-dlt);
                if(cs_e[i]>eps)
                    s_e[i]=min(1.0-eps,s_e[i]+dlt);
                if(cs_e[i]<-eps)
                    s_e[i]=max(eps,s_e[i]-dlt);
            }
            for(int i=0;i<=MAXVERTICLE;++i)
            {
                if(cbeta[i]>eps)
                    beta[i]=min(1.0-eps,beta[i]+dlt);
                if(cbeta[i]<-eps)
                    beta[i]=max(eps,beta[i]-dlt);
            }
            for(int i=0;i<=DOCPERPAGE;++i)
                for(int j=0;j<=DOCPERPAGE;++j)
                {
                    if(cgamma[i][j]>eps)
                        gamma[i][j]=min(1.0-eps,gamma[i][j]+dlt);
                    if(cgamma[i][j]<-eps)
                        gamma[i][j]=max(eps,gamma[i][j]-dlt);
                }
        }
        void train()
        {
            for(int i=0;i<docs.size();++i)
            {
                docs[i].train_tim=0;
            }
            int sess_cnt=0;
            for(auto &sess:sessions)
            {
                if(sess.enable==false)
                    continue;
                ++sess_cnt;
                if(!istrain(sess,sess_cnt))//在没有Overfit可能性的模型里应写作istest，在需要验证集的模型里应写作!istrain
                    continue;
                for(int i=DOCPERPAGE;i;--i)
                    ++docs[sess.doc_id[i]].train_tim;
            }
            double last_ll=-100,now_ll;
            int last_clk;
            train_init();
            for(int round=1;round<=MAXROUND;++round)
            {
                train_clear();
                double maxdlt=0;
                for(auto &sess:sessions)
                {
                    if(sess.enable==false)
                        continue;
                    ++sess_cnt;
                    if(!istrain(sess,sess_cnt))//在没有Overfit可能性的模型里应写作istest，在需要验证集的模型里应写作!istrain
                        continue;
                    get_prob(sess);
                    last_clk=0;
                    double prob_cs=forward[DOCPERPAGE][0]+forward[DOCPERPAGE][1],ptot;
                    for(int i=1;i<=DOCPERPAGE;++i)
                    {
                        int id=sess.doc_id[i];
                        int vid=docs[id].type;
                        if(sess.click_time[i]>.1)
                        {
                            calpha[id]+=1./alpha[id];
                            cbeta[vid]+=1./beta[vid];
                            cgamma[i][i-last_clk]+=1./gamma[i][i-last_clk];
                            cs_c[id]+=(backward[i][1]-backward[i][0])/(s_c[id]*backward[i][1]+(1.-s_c[id])*backward[i][0]);
                            last_clk=i;
                        }
                        else
                        {
                            double &alp=alpha[id],&bet=beta[vid],&gam=gamma[i][i-last_clk],&se=s_e[id];
                            ptot=forward[i-1][1]*backward[i][1]+\
                                forward[i-1][0]*(gam*alp*(1.-bet)*se*backward[i][1]+(1.-alp*gam*(bet+(1.-bet)*se))*backward[i][0]);
                            calpha[id]+=(forward[i-1][0]*gam*(1.-bet)*se*backward[i][1]+forward[i-1][0]*(-gam*bet-gam*(1.-bet)*se)*backward[i][0])/ptot;
                            cbeta[vid]+=(forward[i-1][0]*(0.-alp*gam*se)*backward[i][1]+forward[i-1][0]*(-alp*gam+alp*gam*se)*backward[i][0])/ptot;
                            cgamma[i][i-last_clk]+=(forward[i-1][0]*alp*(1.-bet)*se*backward[i][1]+forward[i-1][0]*(-alp*bet-alp*(1.-bet)*se)*backward[i][0])/ptot;
                            cs_e[id]+=(forward[i-1][0]*gam*(1.-bet)*alp*backward[i][1]+forward[i-1][0]*(0.-gam*(1.-bet)*alp)*backward[i][0])/ptot;
                        }
                    }
                }
                train_update();
                now_ll=this->test(false,2);
                //if(round%20==0)
                    cerr<<"Round:\t"<<round<<"\tLL:\t"<<now_ll<<"\t"<<dlt<<endl;
                last_ll=now_ll;
                dlt*=ddlt;
            }
            for(int i=1;i<=MAXVERTICLE;++i)
                cerr<<beta[i]<<",";
            cerr<<endl;
            /*for(int i=1;i<=DOCPERPAGE;++i)
            {
                for(int j=1;j<=i;++j)
                    cerr<<gamma[i][j]<<"\t";
                cerr<<endl;
            }
            for(int i=1;i<=MAXVERTICLE;++i)
                cerr<<beta[i]<<endl;*/
            /*for(int i=1;i<docs.size();++i)
                if(docs[i].train_tim>10)
                    cerr<<alpha[i]<<"\t"<<s_c[i]<<"\t"<<s_e[i]<<endl;*/
        }
        void load()
        {
        }
        void get_prob(Session &sess)
        {
            static int last_calc_sess=-1;
            if(sess.id==last_calc_sess)
                return;
            last_calc_sess=sess.id;
            memset(forward,0,sizeof(forward));//alpha
            memset(backward,0,sizeof(backward));//beta
            forward[0][0]=1.0;
            forward[0][1]=0.0;
            int last_clk[DOCPERPAGE+2];
            last_clk[0]=0;//last_clk[i]<=i
            //    cerr<<"BEGIN\t"<<sess.id<<endl;
            for(int i=0;i<DOCPERPAGE;++i)
            {
                int id=sess.doc_id[i+1];
                int vid=docs[id].type;
                if(sess.click_time[i+1]>.1)
                {
                    forward[i+1][0]=forward[i][0]*gamma[i+1][i+1-last_clk[i]]*alpha[id]*beta[vid]*(1.-s_c[id]);
                    forward[i+1][1]=forward[i][0]*gamma[i+1][i+1-last_clk[i]]*alpha[id]*beta[vid]*s_c[id];
                    //cerr<<"CLK!\t"<<sess.doc_id[i+1]<<"\t"<<vid<<"\t"<<forward[i+1][0]<<"\t"<<forward[i+1][1]<<"\t"<<gamma[i+1][i+1-last_clk[i]]<<"\t"<<alpha[id]<<"\t"<<beta[vid]<<endl;
                    last_clk[i+1]=i+1;
                }
                else
                {
                    forward[i+1][0]=forward[i][0]*(1.+gamma[i+1][i+1-last_clk[i]]*alpha[id]*((1.-beta[vid])*(1.-s_e[id])-1.));
                    forward[i+1][1]=forward[i][0]*gamma[i+1][i+1-last_clk[i]]*alpha[id]*(1.-beta[vid])*s_e[id]+forward[i][1];
                    //cerr<<"\t\t"<<forward[i+1][0]<<"\t"<<forward[i+1][1]<<"\t"<<gamma[i+1][i+1-last_clk[i]]<<"\t"<<alpha[id]<<"\t"<<beta[vid]<<endl;
                    last_clk[i+1]=last_clk[i];
                }
            }
            backward[DOCPERPAGE][0]=1.0;
            backward[DOCPERPAGE][1]=1.0;
            for(int i=DOCPERPAGE;i;--i)
            {
                int id=sess.doc_id[i];
                int vid=docs[id].type;
                if(sess.click_time[i]>.1)
                {
                    backward[i-1][0]=(backward[i][0]*(1.-s_c[id])+backward[i][1]*s_c[id])*gamma[i][i-last_clk[i-1]]*alpha[id]*beta[vid];
                }
                else
                {
                    backward[i-1][0]=backward[i][0]*(1.+gamma[i][i-last_clk[i-1]]*alpha[id]*((1.-beta[vid])*(1.-s_e[id])-1.))\
                        +backward[i][1]*gamma[i][i-last_clk[i-1]]*alpha[id]*(1.-beta[vid])*s_e[id];
                    backward[i-1][1]=backward[i][1];
                }
            }
        }
        void get_click_prob(Session &sess,double* click_prob)
        {
            //click_prob[i]=P(C_i|C_1,C_2,...C_{i-1})
            //examination = P(E_k|C_1,C_2,...C_{k-1})
            click_prob[0]=1.0;
            double s_prob=0.0;
            int last_clk=0;
            get_prob(sess);
            for(int i=1;i<=DOCPERPAGE;++i)
            {
                int id=sess.doc_id[i];
                int vid=docs[id].type;
                if(sess.click_time[i]>.1)
                {
                    click_prob[i]=forward[i-1][0]/(forward[i-1][0]+forward[i-1][1])*gamma[i][i-last_clk]*alpha[id]*beta[vid];
                    last_clk=i;
                }
                else
                {
                    click_prob[i]=1.-forward[i-1][0]/(forward[i-1][0]+forward[i-1][1])*gamma[i][i-last_clk]*alpha[id]*beta[vid];
                }
            }
        }
};
#endif
