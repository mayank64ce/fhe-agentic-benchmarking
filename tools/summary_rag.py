import os
from pathlib import Path
from typing import List, Optional

from langchain_community.document_loaders import JSONLoader
from langchain.text_splitter import RecursiveCharacterTextSplitter
from langchain_openai import OpenAIEmbeddings
from langchain_chroma import Chroma
from langchain_core.documents import Document

from dotenv import load_dotenv
load_dotenv()


class SummaryRAG:
    """
    Simple RAG over TFHE summaries to fetch method signatures and doxygen docstrings.
    Expects JSON of the form:
      { "summaries": [ {"summary": "...", "method_signature": "...", "doxygen_docstring": "..."}, ... ] }
    """

    def __init__(
        self,
        summary_db_path: str = "../tfhe_documentation/summaries_db.json",
        embedding_model: str = "text-embedding-3-small",
        chunk_size: int = 600,
        chunk_overlap: int = 120,
        persist_directory: str = "./chroma_tfhe_summaries",
        collection_name: str = "tfhe_signatures",
    ):
        self.summary_db_path = Path(summary_db_path)
        if not self.summary_db_path.exists():
            raise FileNotFoundError(f"Summary DB not found at {self.summary_db_path}")

        # OpenAI key (env/ .env)
        if not os.getenv("OPENAI_API_KEY"):
            from dotenv import load_dotenv
            load_dotenv()  # loads .env if present
        if not os.getenv("OPENAI_API_KEY"):
            raise EnvironmentError("OPENAI_API_KEY not set. Export it or add to .env")

        self.embeddings = OpenAIEmbeddings(model=embedding_model)
        self.chunk_size = chunk_size
        self.chunk_overlap = chunk_overlap
        self.persist_directory = persist_directory
        self.collection_name = collection_name

        self._load_documents()
        self._build_or_load_store()

    def _load_documents(self):
        """
        Load JSON and build documents where page_content is the summary + doxygen docstring
        and metadata contains the method_signature and doxygen_docstring.
        """
        # jq extracts exactly the fields we need; content_key decides what is embedded
        loader = JSONLoader(
            file_path=str(self.summary_db_path),
            jq_schema='.summaries[] | {summary: .summary, method_signature: .method_signature, doxygen_docstring: .doxygen_docstring}',
            content_key="summary",
            metadata_func=lambda r, _: {
                "method_signature": r.get("method_signature", ""),
                "doxygen_docstring": r.get("doxygen_docstring", "")
            },
            text_content=True,
        )
        base_docs: List[Document] = loader.load()

        # Include both summary and doxygen docstring in the embedding text for better retrieval
        enriched_docs: List[Document] = []
        for d in base_docs:
            sig = d.metadata.get("method_signature", "")
            doxygen = d.metadata.get("doxygen_docstring", "")
            # Combine summary, doxygen docstring, and signature for comprehensive embedding
            content = f"Doxygen Documentation:\n{doxygen}\n\nSignature: {sig}"
            enriched_docs.append(Document(page_content=content, metadata=d.metadata))

        splitter = RecursiveCharacterTextSplitter(
            chunk_size=self.chunk_size, chunk_overlap=self.chunk_overlap
        )
        self.docs = splitter.split_documents(enriched_docs)

    def _build_or_load_store(self):
        if Path(self.persist_directory).exists():
            self.store = Chroma(
                collection_name=self.collection_name,
                persist_directory=self.persist_directory,
                embedding_function=self.embeddings,
            )
        else:
            self.store = Chroma.from_documents(
                documents=self.docs,
                embedding=self.embeddings,
                collection_name=self.collection_name,
                persist_directory=self.persist_directory,
            )

    def retrieve_signatures(self, query: str, k: int = 3) -> List[str]:
        """
        Return top-k method signatures most relevant to the query.
        """
        # Fast path: if user already typed something that looks like a function name,
        # try exact-ish filtering first.
        if "(" in query and ")" in query:
            # crude filter over all docs' signatures
            # (You could index a small dict for O(1) lookup if you want.)
            all_sigs = {d.metadata.get("method_signature", "") for d in self.docs}
            hits = [s for s in all_sigs if query.split("(")[0] in s]
            if hits:
                return hits[:k]

        results = self.store.similarity_search(query, k=k)
        sigs = []
        for d in results:
            sig = d.metadata.get("method_signature", "")
            if sig and sig not in sigs:
                sigs.append(sig)
        return sigs[:k]

    def retrieve(self, query: str, k: int = 3):
        """
        Return (signature, summary_text, doxygen_docstring) tuples for top-k.
        """
        results = self.store.similarity_search(query, k=k)
        out = []
        for d in results:
            sig = d.metadata.get("method_signature", "")
            doxygen = d.metadata.get("doxygen_docstring", "")
            # Strip the synthetic content added to page_content
            cleaned_summary = d.page_content.split("\n\nDoxygen Documentation:")[0].strip()
            out.append((sig, cleaned_summary, doxygen))
        return out

    def retrieve_doxygen_docs(self, query: str, k: int = 3) -> List[str]:
        """
        Return top-k doxygen docstrings most relevant to the query.
        """
        results = self.store.similarity_search(query, k=k)
        docs = []
        for d in results:
            doxygen = d.metadata.get("doxygen_docstring", "")
            if doxygen and doxygen not in docs:
                docs.append(doxygen)
        return docs[:k]


if __name__ == "__main__":
    rag = SummaryRAG(
        summary_db_path="../tfhe_documentation/summaries_db.json",
        embedding_model="text-embedding-3-small",
        persist_directory="./chroma_tfhe_summaries",
    )

    tests = [
        "bitwise AND of 2 integers",
        # "homomorphic multiplexer (a ? b : c)",
        # "create default bootstrapping parameters",
        # "export cloud key to a stream",
        # "bitwise and gate on encrypted bits",
    ]
    
    print("=== Testing signature retrieval ===")
    for q in tests:
        print(f"\nQ: {q}")
        # for (sig, summary, doxygen) in rag.retrieve(q, k=2):
        #     retrived = "\n".join([doxygen, sig])
        #     print("  →", retrived)
        sig, summary, doxygen = rag.retrieve(q, k=1)[0]
        retrived = "\n".join([doxygen, sig])
        print("  →", retrived)
