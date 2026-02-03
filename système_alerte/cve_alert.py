import requests
import csv
import time
import smtplib
import os
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from dotenv import load_dotenv 

load_dotenv()

CSV_FILE = "/Users/simon/Desktop/stack.csv"
EMAIL_SENDER = os.getenv("EMAIL_SENDER")
EMAIL_PASSWORD = os.getenv("EMAIL_PASSWORD") 
EMAIL_RECEIVER = os.getenv("EMAIL_RECEIVER")

def get_eol_and_updates(slug, current_version):
    url = f"https://endoflife.date/api/{slug}.json"
    try:
        response = requests.get(url, timeout=10)
        if response.status_code != 200: return None
        cycles = response.json()
        for cycle in cycles:
            if str(cycle['cycle']) in current_version:
                return cycle
        return None
    except: return None

def get_cve_details(product, version):
    url = f"https://services.nvd.nist.gov/rest/json/cves/2.0?keywordSearch={product} {version}"
    try:
        response = requests.get(url, timeout=10)
        data = response.json()
        vulnerabilities = data.get('vulnerabilities', [])
        count = len(vulnerabilities)
        
        if not vulnerabilities:
            return "   ✅ SÉCURITÉ : Aucune vulnérabilité critique.\n", 0

        report = f"   🛑 SÉCURITÉ : {count} vulnérabilités trouvées\n"
        for item in vulnerabilities:
            cve_id = item['cve']['id']
            score = "N/A"
            severity = "Inconnue"
            metrics = item['cve'].get('metrics', {})
            if 'cvssMetricV31' in metrics:
                m = metrics['cvssMetricV31'][0]['cvssData']
                score = m['baseScore']
                severity = m.get('baseSeverity', "N/A")

            report += f"      - {cve_id} [{severity} | Score: {score}]\n"
        return report, count
    except:
        return "   ⚠️ SÉCURITÉ : Erreur API.\n", 0

def send_email(content):
    if not EMAIL_SENDER or not EMAIL_PASSWORD:
        print("❌ Erreur : Identifiants manquants dans le fichier .env")
        return False

    msg = MIMEMultipart()
    msg['From'] = EMAIL_SENDER
    msg['To'] = EMAIL_RECEIVER
    msg['Subject'] = "🚨 Rapport de Sécurité Stack & EoL"
    msg.attach(MIMEText(content, 'plain'))
    
    try:
        server = smtplib.SMTP('smtp.gmail.com', 587)
        server.starttls()
        server.login(EMAIL_SENDER, EMAIL_PASSWORD)
        server.send_message(msg)
        server.quit()
        return True
    except Exception as e:
        print(f"❌ Erreur SMTP : {e}")
        return False

def run_audit():
    print(f"\n🔍 DÉMARRAGE DE L'AUDIT POUR : {EMAIL_RECEIVER}")
    full_report = "--- RAPPORT D'AUDIT TECHNIQUE ---\n\n"
    
    try:
        with open(CSV_FILE, mode='r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                full_report += f"📦 PRODUIT : {row['produit'].upper()} (v{row['version']})\n"
                cve_text, _ = get_cve_details(row['produit'], row['version'])
                full_report += cve_text
                
                eol_info = get_eol_and_updates(row['slug'], row['version'])
                if eol_info:
                    full_report += f"   📅 FIN DE VIE : {eol_info.get('eol')}\n"
                    full_report += f"   🛠 SOLUTION : Maj vers v{eol_info.get('latest')}\n"
                full_report += "\n" + "-"*40 + "\n\n"
                time.sleep(2) # Politesse pour l'API NVD

        if send_email(full_report):
            print("✨ SUCCÈS : Rapport envoyé !")
    except Exception as e:
        print(f"❌ Erreur : {e}")

if __name__ == "__main__":
    run_audit()